#define NOMINMAX
#include "GameScene.h"

#include "Input.h"
#include "ModelRenderer.h"
#include "BufferUtility.h"
#include "clearScene.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <DirectXTex/d3dx12.h>
#include <DirectXTex/DirectXTex.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace {
    constexpr float kBlockDepth = 0.5f;
    constexpr float kPlayerDepth = 0.6f;
    constexpr float kPlayerZ = -0.26f; // -Z が手前に来る

    constexpr float kFragileBreakTime = 1.35f; // armed→完全消滅
    constexpr float kRegenRespawnTime = 2.0f;  // 再生床が戻るまで
    constexpr float kFragileBlinkStart = 0.0f;  // 点滅開始のリードタイム
    constexpr float kFragileBlinkFreq = 4.0f;  // Hz
}

// ====== 壊れ床の点滅アルファ ======
float GameScene::FragileBlinkFactor_(int tx, int ty) const {
    if (!InMap(tx, ty)) {
        return 1.0f;
    }

    Tile t = grid_[ty][tx];
    if (!IsFragile(t)) {
        return 1.0f;
    }

    const FragileState &fs = frag_[ty][tx];

    // もう消滅してたら描かない
    if (fs.gone) {
        return 0.0f;
    }

    // まだarmedじゃない → 常に等倍表示 (点滅なし)
    if (!fs.armed) {
        return 1.0f;
    }

    // ---- ここからarmed中の見た目 ----
    // 要求：
    // ・壊れる直前まで同じテンポ＆同じ明るさパターンで点滅
    // ・時間経過でどんどん暗くなったりしない

    // 周期だけ使う（fs.tに応じてチカチカするけど、強さは一定）
    float elapsed = fs.t;

    // 周期(Hz) → 1/freq 秒のサイクル
    float period = 1.0f / kFragileBlinkFreq; // 4Hzなら0.25秒で1サイクル
    float cyclePos = std::fmodf(elapsed, period);
    float t01 = cyclePos / period; // 0→1

    // コサイン波 (0→1→0)。 0〜1の点滅カーブ。
    float wave = 0.5f * (1.0f - std::cos(t01 * 2.0f * 3.14159265f));

    // 一定の明滅レンジに固定する
    // 例えば 0.4〜1.0 の間で点滅させる
    // （あまり0に近いと完全に消えて見失うのでゲーム的にキツい）
    constexpr float kMinAlpha = 0.4f;
    constexpr float kMaxAlpha = 1.0f;

    float alpha = kMinAlpha + (kMaxAlpha - kMinAlpha) * wave;

    return std::clamp(alpha, 0.0f, 1.0f);
}


// ====== UTF-8 → UTF-16 ======
std::wstring GameScene::Widen_(const std::string &u8) {
    if (u8.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// ====== 入力立ち上がり ======
bool GameScene::KeyPressed_(uint8_t dik) {
    auto *in = engineContext_->input;
    bool now = in->IsKeyPressed(dik);
    bool was = keyPrev_[dik] != 0;
    keyPrev_[dik] = now ? 1 : 0;
    return now && !was;
}

// ====== 属性 ======
bool GameScene::IsFragile(Tile t) {
    return t == Tile::FragileAny
        || t == Tile::FragileTop
        || t == Tile::FragileBottom
        || t == Tile::Regen;
}
bool GameScene::IsSpring(Tile t) {
    return t == Tile::Spring;
}

// 「このタイルはプレイヤーが床として乗れる/ぶつかる？」
// ※スイッチ本体(Tile::Switch)は床じゃないのでfalse扱い
bool GameScene::IsBlockingAt(int tx, int ty) const {
    if (!InMap(tx, ty)) return false;
    Tile t = grid_[ty][tx];

    // 壊れる床/再生床
    if (IsFragile(t)) {
        if (frag_[ty][tx].gone) return false;
        return true;
    }

    // スイッチ連動床
    // ON 床は switchOn_ == true のときだけ実体
    // OFF 床は switchOn_ == false のときだけ実体
    if (t == Tile::SwitchBlockOn)  return switchOn_;
    if (t == Tile::SwitchBlockOff) return !switchOn_;

    // 通常足場
    if (t == Tile::JumpOnly) return true;
    if (t == Tile::Solid)    return true;

    // スイッチ本体 / スプリング / スパイク / etc. は床じゃない
    return false;
}


// ====== マップ初期化 ======
void GameScene::ResetGrid() {
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            grid_[y][x] = Tile::Empty;
            frag_[y][x] = FragileState{};
            regen_[y][x] = RegenState{};
        }
    }
    switchOn_ = false;
    spawnTx_ = 2;
    spawnTy_ = 2;
}

// ====== サンプルマップ ======
void GameScene::BuildSample() {
    ResetGrid();

    // 地面
    for (int x = 0; x < kMapW; ++x) {
        grid_[kMapH - 2][x] = Tile::Solid;
    }

    // 壊れる床群
    for (int x = 3; x <= 8; ++x) grid_[kMapH - 5][x] = Tile::FragileAny;
    for (int x = 11; x <= 14; ++x) grid_[kMapH - 7][x] = Tile::FragileTop;
    for (int x = 16; x <= 19; ++x) grid_[kMapH - 9][x] = Tile::FragileBottom;

    // ギミック
    grid_[kMapH - 3][6] = Tile::Spring;

    // スイッチ本体
    grid_[kMapH - 6][18] = Tile::Switch;

    // スイッチ連動床（青＝ON時だけ / 赤＝OFF時だけ）
    grid_[kMapH - 6][20] = Tile::SwitchBlockOn;
    grid_[kMapH - 6][21] = Tile::SwitchBlockOn;
    grid_[kMapH - 6][23] = Tile::SwitchBlockOff;

    // スパイク
    for (int x = kMapW - 8; x < kMapW - 2; ++x) {
        grid_[kMapH - 3][x] = Tile::Spike;
    }

    // 復活床
    grid_[kMapH - 8][22] = Tile::Regen;
}

// ====== CSV保存 ======
bool GameScene::SaveCSV(const std::string &path) const {
    std::ofstream ofs(path);
    if (!ofs) return false;

    ofs << kMapW << "," << kMapH << "," << spawnTx_ << "," << spawnTy_ << "\n";
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            ofs << (int)grid_[y][x];
            if (x + 1 < kMapW) ofs << ",";
        }
        ofs << "\n";
    }
    return true;
}

// ====== CSV読み込み ======
bool GameScene::LoadCSV(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs) return false;

    ResetGrid();

    std::string line;
    if (!std::getline(ifs, line)) return false;

    // 1行目: "W,H,spawnTx,spawnTy"
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ssHeader(line);

        std::string tok;
        std::vector<int> vals;
        while (std::getline(ssHeader, tok, ',')) {
            int v = 0;
            if (!tok.empty()) {
                try { v = std::stoi(tok); }
                catch (...) { v = 0; }
            }
            vals.push_back(v);
        }
        if (vals.size() >= 4) {
            spawnTx_ = std::clamp(vals[2], 0, kMapW - 1);
            spawnTy_ = std::clamp(vals[3], 0, kMapH - 1);
        }
    }

    int y = 0;
    while (y < kMapH && std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);

        std::string cell;
        int x = 0;
        while (x < kMapW && std::getline(ss, cell, ',')) {
            int id = 0;
            if (!cell.empty()) {
                try { id = std::stoi(cell); }
                catch (...) { id = 0; }
            }
            id = std::clamp(id, 0, (int)Tile::SwitchBlockOff);
            Tile t = (Tile)id;
            grid_[y][x] = t;
            if (IsFragile(t)) frag_[y][x] = FragileState{};
            if (t == Tile::Regen) regen_[y][x] = RegenState{};
            ++x;
        }
        ++y;
    }

    ClampSpawnToSafe();
    return true;
}

// ====== スポーン位置補正 ======
void GameScene::ClampSpawnToSafe() {
    int tx = std::clamp(spawnTx_, 0, kMapW - 1);
    int ty = std::clamp(spawnTy_, 0, kMapH - 1);
    if (IsBlockingAt(tx, ty)) {
        // もし詰まってたら上にずらす
        for (int r = ty - 1; r >= 0; --r) {
            if (!IsBlockingAt(tx, r)) { ty = r; break; }
        }
    }
    spawnTx_ = tx;
    spawnTy_ = ty;
}

// ====== テクスチャ読み込み + SRV ======
bool GameScene::LoadTextureSRV_(
    const std::wstring &fileU16,
    UINT srvIndex,
    ComPtr<ID3D12Resource> &outTex,
    D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle) {

    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();
    if (!device || fileU16.empty()) return false;

    DirectX::ScratchImage img, conv;
    HRESULT hr = DirectX::LoadFromWICFile(
        fileU16.c_str(),
        DirectX::WIC_FLAGS_FORCE_SRGB,
        nullptr,
        img
    );
    if (FAILED(hr)) return false;

    const DirectX::TexMetadata &meta = img.GetMetadata();
    DXGI_FORMAT targetFmt = meta.format;

    if (!DirectX::IsCompressed(meta.format) && !DirectX::IsSRGB(meta.format)) {
        hr = DirectX::Convert(
            img.GetImages(), img.GetImageCount(), meta,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DirectX::TEX_FILTER_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT,
            conv
        );
        if (FAILED(hr)) return false;
    }

    const DirectX::Image *srcImgs = conv.GetImages() ? conv.GetImages() : img.GetImages();
    DirectX::TexMetadata useMeta = conv.GetMetadata().width ? conv.GetMetadata() : meta;
    targetFmt = useMeta.format;

    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = static_cast<UINT>(useMeta.width);
    texDesc.Height = static_cast<UINT>(useMeta.height);
    texDesc.DepthOrArraySize = static_cast<UINT16>(useMeta.arraySize);
    texDesc.MipLevels = static_cast<UINT16>(useMeta.mipLevels ? useMeta.mipLevels : 1);
    texDesc.Format = targetFmt;
    texDesc.SampleDesc = {1, 0};
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapDef{};
    heapDef.Type = D3D12_HEAP_TYPE_DEFAULT;

    hr = device->CreateCommittedResource(
        &heapDef,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&outTex));
    if (FAILED(hr)) return false;

    UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);
    ComPtr<ID3D12Resource> upload = BufferUtility::CreateUploadBuffer(device, uploadSize);

    // ワンショットコマンドで転送
    ComPtr<ID3D12CommandQueue> queue;
    {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    }
    ComPtr<ID3D12CommandAllocator> alloc;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));

    ComPtr<ID3D12GraphicsCommandList> list;
    device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        alloc.Get(),
        nullptr,
        IID_PPV_ARGS(&list)
    );

    {
        std::vector<D3D12_SUBRESOURCE_DATA> subs(static_cast<size_t>(useMeta.mipLevels));
        for (size_t m = 0; m < useMeta.mipLevels; ++m) {
            const DirectX::Image &im = srcImgs[m];
            subs[m].pData = im.pixels;
            subs[m].RowPitch = im.rowPitch;
            subs[m].SlicePitch = im.slicePitch;
        }

        UpdateSubresources(
            list.Get(), outTex.Get(), upload.Get(),
            0, 0, static_cast<UINT>(useMeta.mipLevels),
            subs.data()
        );

        auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
            outTex.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        list->ResourceBarrier(1, &toSRV);
    }

    list->Close();
    ID3D12CommandList *lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    queue->Signal(fence.Get(), 1);
    if (fence->GetCompletedValue() < 1) {
        fence->SetEventOnCompletion(1, evt);
        WaitForSingleObject(evt, INFINITE);
    }
    CloseHandle(evt);

    // SRVをヒープに登録
    ID3D12DescriptorHeap *srvHeap = engineContext_->directXCommon->GetSrvHeap();
    const UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += SIZE_T(inc) * srvIndex;

    D3D12_GPU_DESCRIPTOR_HANDLE gpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(inc) * srvIndex;
    outGpuHandle = gpu;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = targetFmt;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = texDesc.MipLevels;

    device->CreateShaderResourceView(outTex.Get(), &srv, cpu);
    return true;
}

// ====== Initialize ======
void GameScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
    engineContext_ = engineContext;
    renderContext_ = renderContext;
    sceneManager_ = engineContext_->sceneManager;

    cleared_ = false;
    elapsedTime_ = 0.0f;
    finalTime_ = 0.0f;

    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // --- モデル読み込み ---
    playerModel_.Initialize(device, "Resources/Model/Player/player.obj");
    mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");

    mdlFragileAny_.Initialize(device, "Resources/Model/Block/fragile_any.obj");
    mdlFragileTop_.Initialize(device, "Resources/Model/Block/fragile_top.obj");
    mdlFragileBottom_.Initialize(device, "Resources/Model/Block/fragile_bottom.obj");

    mdlRegen_.Initialize(device, "Resources/Model/Block/regen.obj");
    mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
    mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");

    // スイッチ本体モデル（押すボタン）
    mdlSwitchOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");

    // スイッチ連動床モデル（足場）
    // 別のモデルがまだ無いなら、とりあえず同じobjを指定しておいてOK
    mdlSwitchBlockOn_.Initialize(device, "Resources/Model/Block/switchblock_on.obj");
    mdlSwitchBlockOff_.Initialize(device, "Resources/Model/Block/switchblock_off.obj");

    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");

    // --- 各モデルのアルベドテクスチャをSRVに登録 ---
    auto setupTex = [&](Model &m, UINT slot, ComPtr<ID3D12Resource> &holder) {
        if (!m.GetAlbedoPath().empty()) {
            D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
            if (LoadTextureSRV_(Widen_(m.GetAlbedoPath()), slot, holder, gpu)) {
                m.SetAlbedoSRV(gpu);
            }
        }
        };

    setupTex(playerModel_, kSrvIndex_Player, texPlayer_);
    setupTex(mdlSolid_, kSrvIndex_Solid, texSolid_);
    setupTex(mdlFragileAny_, kSrvIndex_FragileAny, texFragileAny_);
    setupTex(mdlFragileTop_, kSrvIndex_FragileTop, texFragileTop_);
    setupTex(mdlFragileBottom_, kSrvIndex_FragileBottom, texFragileBottom_);
    setupTex(mdlRegen_, kSrvIndex_Regen, texRegen_);
    setupTex(mdlSpring_, kSrvIndex_Spring, texSpring_);
    setupTex(mdlSpike_, kSrvIndex_Spike, texSpike_);

    // スイッチ本体のON/OFF
    setupTex(mdlSwitchOn_, kSrvIndex_SwitchOn, texSwitchOn_);
    setupTex(mdlSwitchOff_, kSrvIndex_SwitchOff, texSwitchOff_);

    // スイッチ連動床のON/OFF
    setupTex(mdlSwitchBlockOn_, kSrvIndex_SwitchBlockOn, texSwitchBlockOn_);
    setupTex(mdlSwitchBlockOff_, kSrvIndex_SwitchBlockOff, texSwitchBlockOff_);

    setupTex(mdlJumpOnly_, kSrvIndex_JumpOnly, texJumpOnly_);

    // === マップオフセット（横方向センタリング） ===
    const float mapW = kMapW * kTile;
    xOffset_ = -mapW * 0.5f;

    // === ステージCSVロード ===
    std::string stagePath;
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "stage%02d.csv", stageId_);
        stagePath = buf;
    }
    if (!LoadCSV(stagePath)) {
        if (!LoadCSV("stage.csv")) {
            BuildSample();
            (void)SaveCSV(stagePath); // ないならサンプルを出力
        }
    }
    ClampSpawnToSafe();

    // === プレイヤー初期配置 ===
    playerTr_ = {};
    playerTr_.scale = {1,1,1};
    playerTr_.pos = {
        xOffset_ + spawnTx_ * kTile,
        TyToWorldY(spawnTy_) + 0.5f,
        kPlayerZ
    };

    vel_ = {0,0,0};
    onGround_ = false;
    coyoteCounter_ = 0;
    jumpBuffer_ = 0;
    std::memset(keyPrev_, 0, sizeof(keyPrev_));

    // === カメラ設定（マップ全体を正射影で収める） ===
    {
        const float worldW = kMapW * kTile;
        const float worldH = kMapH * kTile;

        float screenW = (float)dx->GetWidth();
        float screenH = (float)dx->GetHeight();
        float screenAspect = screenW / std::max(1.0f, screenH);
        float mapAspect = worldW / worldH;

        float orthoW, orthoH;
        if (screenAspect >= mapAspect) {
            orthoH = worldH;
            orthoW = worldH * screenAspect;
        } else {
            orthoW = worldW;
            orthoH = worldW / screenAspect;
        }

        const float centerX = 0.0f;
        const float centerY = worldH * 0.5f;
        const float camZ = -50.0f;

        camera_.Initialize(
            {centerX, centerY, camZ},
            {0.0f,    0.0f,    0.0f},
            60.0f,
            screenAspect,
            0.1f,
            1000.0f
        );
        camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);
        camera_.LookAt(
            {centerX, centerY, camZ},
            {centerX, centerY, 0.0f}
        );
        camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    }

    // === 初期スナップ ===
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            initialGrid_[y][x] = grid_[y][x];
            initialFrag_[y][x] = frag_[y][x];
            initialRegen_[y][x] = regen_[y][x];
        }
    }
    initialSwitchOn_ = switchOn_;
    initialSpawnTx_ = spawnTx_;
    initialSpawnTy_ = spawnTy_;
}

// ====== 横方向の物理解決 ======
void GameScene::ResolveHorizontal_() {
    if (vel_.x == 0.0f) return;

    float startX = playerTr_.pos.x;
    float targetX = startX + vel_.x;

    AABB boxNow = PlayerAabbFull_();

    const float minY = boxNow.y + kSkinY;
    const float maxY = boxNow.y + boxNow.h - 1e-4f;

    int tyBottom = ToTy(minY);
    int tyTop = ToTy(maxY);
    int tyMin = std::min(tyBottom, tyTop);
    int tyMax = std::max(tyBottom, tyTop);

    bool  collided = false;
    float stopX = targetX;

    if (vel_.x > 0.0f) {
        // 右
        float startRight = startX + pw_;
        float endRight = targetX + pw_;

        int colStart = ToTx(startRight - 1e-4f);
        int colEnd = ToTx(endRight - 1e-4f);
        if (colEnd < colStart) colEnd = colStart;

        for (int col = colStart; col <= colEnd; ++col) {
            for (int ty = tyMin; ty <= tyMax; ++ty) {
                if (!InMap(col, ty)) continue;
                if (!IsBlockingAt(col, ty)) continue;

                float bx = xOffset_ + col * kTile;
                float by = TyToWorldY(ty);

                float overlapY = std::min(maxY, by + kTile) - std::max(minY, by);
                if (overlapY <= 0.0f) continue;

                float wallLeft = bx;
                if (endRight > wallLeft) {
                    float candidate = wallLeft - pw_ - kSkinX;
                    if (!collided || candidate < stopX) {
                        collided = true;
                        stopX = candidate;
                    }
                }
            }
        }
    } else {
        // 左
        float startLeft = startX;
        float endLeft = targetX;

        int colStart = ToTx(startLeft + 1e-4f);
        int colEnd = ToTx(endLeft + 1e-4f);
        if (colEnd > colStart) colEnd = colStart;

        for (int col = colStart; col >= colEnd; --col) {
            for (int ty = tyMin; ty <= tyMax; ++ty) {
                if (!InMap(col, ty)) continue;
                if (!IsBlockingAt(col, ty)) continue;

                float bx = xOffset_ + col * kTile;
                float by = TyToWorldY(ty);

                float overlapY = std::min(maxY, by + kTile) - std::max(minY, by);
                if (overlapY <= 0.0f) continue;

                float wallRight = bx + kTile;
                if (endLeft < wallRight) {
                    float candidate = wallRight + kSkinX;
                    if (!collided || candidate > stopX) {
                        collided = true;
                        stopX = candidate;
                    }
                }
            }
        }
    }

    if (collided) {
        playerTr_.pos.x = stopX;
        vel_.x = 0.0f;
    } else {
        playerTr_.pos.x = targetX;
    }
}

// ====== 縦方向の物理解決＋ギミック＋死亡判定 ======
void GameScene::ResolveVertical_(float dt) {
    float startY = playerTr_.pos.y;
    float targetY = startY + vel_.y;

    AABB boxNow = PlayerAabbFull_();

    // X方向の列範囲
    float minX = boxNow.x;
    float maxX = boxNow.x + boxNow.w - 1e-4f;
    int txL = ToTx(minX);
    int txR = ToTx(maxX);
    int txMin = std::min(txL, txR);
    int txMax = std::max(txL, txR);

    onGround_ = false;

    // このフレーム、スイッチに触れているか？
    bool switchOverlapNow = false;

    // スプリングを踏んだ（＝バネで吹っ飛ぶべき）か？
    bool springBounce = false;

    // 未来位置のAABB（Yだけ更新後の想定）
    AABB afterBox{boxNow.x, targetY, boxNow.w, boxNow.h};

    if (vel_.y <= 0.0f) {
        // ===== 落下・着地 =====
        float startBottom = startY;
        float endBottom = targetY;

        int rowStart = ToTy(startBottom - kSkinY);
        int rowEnd = ToTy(endBottom - kSkinY);
        int rowMin = std::min(rowStart, rowEnd);
        int rowMax = std::max(rowStart, rowEnd);

        bool  hitFloor = false;
        float bestSnapY = targetY;

        for (int row = rowMin; row <= rowMax; ++row) {
            for (int tx = txMin; tx <= txMax; ++tx) {
                if (!InMap(tx, row)) continue;

                Tile tt = grid_[row][tx];
                float bx = xOffset_ + tx * kTile;
                float by = TyToWorldY(row);
                float topY = by + kTile;

                // ===== スプリング判定（どこから触れても即発火） =====
                if (IsSpring(tt)) {
                    if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
                        // ここでは直接 vel_.y をいじらず、
                        // 「このフレームはバネで跳ねるべき」という意思だけ残す
                        springBounce = true;
                    }
                }

                // ===== スイッチ本体判定（踏んでるかどうか） =====
                if (tt == Tile::Switch) {
                    if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
                        switchOverlapNow = true;
                    }
                }

                if (!IsBlockingAt(tx, row)) continue;

                // 真下から床に乗る衝突チェック
                if ((startBottom - kSkinY) >= topY &&
                    (endBottom - kSkinY) < topY) {

                    float overlapX =
                        std::min(boxNow.x + boxNow.w, bx + kTile)
                        - std::max(boxNow.x, bx);

                    if (overlapX > kMinGroundOverlap) {
                        float snapY = topY + kSkinY;
                        if (!hitFloor || snapY > bestSnapY) {
                            hitFloor = true;
                            bestSnapY = snapY;

                            // fragile踏んだらarmed
                            if (IsFragile(tt) && !frag_[row][tx].gone) {
                                if (tt == Tile::FragileAny ||
                                    tt == Tile::FragileTop ||
                                    tt == Tile::Regen) {
                                    ArmFragile_(tx, row);
                                }
                            }
                        }
                    }
                }
            }
        }

        // ここで最終決定：
        if (springBounce) {
            // バネ優先：上方向に吹っ飛ぶ
            vel_.y = kSpringVy;
            playerTr_.pos.y = targetY; // スナップしないで継続位置に
            onGround_ = false;
        } else if (hitFloor) {
            // ふつうの着地
            playerTr_.pos.y = bestSnapY;
            vel_.y = 0.0f;
            onGround_ = true;
        } else {
            // ただ落下継続
            playerTr_.pos.y = targetY;
        }
    } else {
        // ===== 上昇・頭ぶつけ =====
        float startTop = startY + ph_;
        float endTop = targetY + ph_;

        int rowStart = ToTy(startTop + kSkinY);
        int rowEnd = ToTy(endTop + kSkinY);
        int rowMin = std::min(rowStart, rowEnd);
        int rowMax = std::max(rowStart, rowEnd);

        bool  hitCeil = false;
        float bestSnapY = targetY;

        for (int row = rowMin; row <= rowMax; ++row) {
            for (int tx = txMin; tx <= txMax; ++tx) {
                if (!InMap(tx, row)) continue;

                Tile tt = grid_[row][tx];
                float bx = xOffset_ + tx * kTile;
                float by = TyToWorldY(row);
                float bottomY = by;

                // ===== スプリング判定（どこから触れても即発火） =====
                if (IsSpring(tt)) {
                    if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
                        vel_.y = kSpringVy;
                    }
                }

                // 下からfragile壊す(頭ゴン)
                if (IsFragile(tt) && !frag_[row][tx].gone) {
                    bool canFromBelow =
                        (tt == Tile::FragileAny ||
                            tt == Tile::FragileBottom ||
                            tt == Tile::Regen);

                    if (canFromBelow) {
                        float overlapX =
                            std::min(boxNow.x + boxNow.w, bx + kTile)
                            - std::max(boxNow.x, bx);

                        if (overlapX > kMinGroundOverlap) {
                            if ((startTop + kSkinY) <= bottomY &&
                                (endTop + kSkinY) > bottomY) {

                                ArmFragile_(tx, row);
                            }
                        }
                    }
                }

                // 空中でスイッチにヒットする場合
                if (tt == Tile::Switch) {
                    if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
                        switchOverlapNow = true;
                    }
                }

                if (!IsBlockingAt(tx, row)) continue;

                // 頭が天井にぶつかった？
                if ((startTop + kSkinY) <= bottomY &&
                    (endTop + kSkinY) > bottomY) {

                    float overlapX =
                        std::min(boxNow.x + boxNow.w, bx + kTile)
                        - std::max(boxNow.x, bx);

                    if (overlapX > kMinGroundOverlap) {
                        float snapY = bottomY - ph_ - kSkinY;
                        if (!hitCeil || snapY < bestSnapY) {
                            hitCeil = true;
                            bestSnapY = snapY;
                        }
                    }
                }
            }
        }

        if (hitCeil) {
            playerTr_.pos.y = bestSnapY;
            vel_.y = 0.0f;
        } else {
            playerTr_.pos.y = targetY;
        }
    }

    // 足元かすり接地でfragile armed付与（既存ロジックそのまま）
    {
        int txL2 = ToTx(playerTr_.pos.x);
        int txR2 = ToTx(playerTr_.pos.x + pw_ - 1e-4f);
        int txMin2 = std::min(txL2, txR2);
        int txMax2 = std::max(txL2, txR2);

        int rowBelow = ToTy(playerTr_.pos.y - kSkinY);

        for (int tx = txMin2; tx <= txMax2; ++tx) {
            if (!InMap(tx, rowBelow)) continue;
            Tile tt = grid_[rowBelow][tx];
            if (!IsFragile(tt) || frag_[rowBelow][tx].gone) continue;

            if (!(tt == Tile::FragileAny ||
                tt == Tile::FragileTop ||
                tt == Tile::Regen)) {
                continue;
            }

            float bx = xOffset_ + tx * kTile;
            float overlapX =
                std::min(playerTr_.pos.x + pw_, bx + kTile)
                - std::max(playerTr_.pos.x, bx);

            if (overlapX > kMinGroundOverlap) {
                ArmFragile_(tx, rowBelow);
            }
        }
    }

    // ==== スイッチトグル処理（1フレーム1回だけ） ====
    if (!wasOnSwitch_ && switchOverlapNow) {
        switchOn_ = !switchOn_;
    }
    wasOnSwitch_ = switchOverlapNow;

    // ===== コヨーテ/ジャンプバッファ =====
    if (onGround_) {
        coyoteCounter_ = kCoyoteMaxFrames;
    } else if (coyoteCounter_ > 0) {
        --coyoteCounter_;
    }

    if (jumpBuffer_ > 0) --jumpBuffer_;

    if ((onGround_ || coyoteCounter_ > 0) && jumpBuffer_ > 0) {
        vel_.y = kJumpVy;
        onGround_ = false;
        jumpBuffer_ = 0;
    }

    // 接地スナップ（地上扱いのときだけ）
    if (onGround_) {
        float stableY =
            std::floor((playerTr_.pos.y - kSkinY) / kTile) * kTile + kSkinY;
        if (std::fabs(playerTr_.pos.y - stableY) > 1e-4f) {
            playerTr_.pos.y = stableY;
        }
    }

    // fragile / regen タイマー進行（元の処理そのまま）
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            Tile t = grid_[y][x];
            if (!IsFragile(t)) continue;

            auto &fs = frag_[y][x];
            if (!fs.gone) {
                if (fs.armed) {
                    fs.t += dt;
                    if (fs.t > kFragileBreakTime) {
                        fs.gone = true;
                        if (t == Tile::Regen) {
                            regen_[y][x].respawn = 0.0f;
                        }
                    }
                }
            } else if (t == Tile::Regen) {
                regen_[y][x].respawn += dt;
                if (regen_[y][x].respawn >= kRegenRespawnTime) {
                    fs = FragileState{}; // 復活
                }
            }
        }
    }

    // --- デス判定（元の処理そのまま） ---
    {
        bool killed = false;

        // 奈落
        float deathY = -4.0f * kTile;
        if (playerTr_.pos.y < deathY) {
            killed = true;
        }

        // スパイク
        if (!killed) {
            AABB f = PlayerAabbFull_();
            int l3 = ToTx(f.x);
            int r3 = ToTx(f.x + f.w - 1e-4f);
            int t3 = ToTy(f.y);
            int b3 = ToTy(f.y + f.h - 1e-4f);

            int tyMin3 = std::min(t3, b3);
            int tyMax3 = std::max(t3, b3);
            int txMin3 = std::min(l3, r3);
            int txMax3 = std::max(l3, r3);

            for (int ty = tyMin3; ty <= tyMax3 && !killed; ++ty) {
                for (int tx = txMin3; tx <= txMax3; ++tx) {
                    if (!InMap(tx, ty)) continue;
                    if (grid_[ty][tx] != Tile::Spike) continue;

                    float bx = xOffset_ + tx * kTile;
                    float by = TyToWorldY(ty);

                    if (OverlapXY(f, bx, by, kTile, kTile)) {
                        killed = true;
                        break;
                    }
                }
            }
        }

        if (killed) {
            ResetStageAll_();
            return;
        }
    }
}





// ====== リセット ======
void GameScene::ResetStageAll_() {
    // マップ/壊れ床/再生床/スイッチ状態/スポーン位置を初期に戻す
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            grid_[y][x] = initialGrid_[y][x];
            frag_[y][x] = initialFrag_[y][x];
            regen_[y][x] = initialRegen_[y][x];
        }
    }
    switchOn_ = initialSwitchOn_;
    spawnTx_ = initialSpawnTx_;
    spawnTy_ = initialSpawnTy_;

    ClampSpawnToSafe();

    // プレイヤー再配置
    playerTr_.pos = {
        xOffset_ + spawnTx_ * kTile,
        TyToWorldY(spawnTy_) + 0.5f,
        kPlayerZ
    };
    vel_ = {0,0,0};
    onGround_ = false;
    coyoteCounter_ = 0;
    jumpBuffer_ = 0;
}

// ====== Update ======
void GameScene::Update(float dt) {
    auto *dx = engineContext_->directXCommon;
    auto *in = engineContext_->input;

    if (!cleared_) {
        elapsedTime_ += dt;
    }

    // 画面リサイズ時にカメラのOrthoサイズを追従
    {
        float screenW = (float)dx->GetWidth();
        float screenH = (float)dx->GetHeight();
        float screenAspect = screenW / std::max(1.0f, screenH);

        const float worldW = kMapW * kTile;
        const float worldH = kMapH * kTile;
        float mapAspect = worldW / worldH;

        float orthoW, orthoH;
        if (screenAspect >= mapAspect) {
            orthoH = worldH;
            orthoW = worldH * screenAspect;
        } else {
            orthoW = worldW;
            orthoH = worldW / screenAspect;
        }

        if (camera_.IsOrtho()) {
            camera_.SetOrthoViewSize(orthoW, orthoH);
        }
        camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    }

    // 入力
    int ax = 0;
    if (in->IsKeyPressed(DIK_A)) ax -= 1;
    if (in->IsKeyPressed(DIK_D)) ax += 1;
    if (KeyPressed_(DIK_SPACE))  jumpBuffer_ = kJumpBufferFrames;

    // 横速度
    vel_.x = (onGround_ ? kMoveGround : kMoveAir) * (float)ax;

    // 重力
    vel_.y += -kGravity;
    if (vel_.y < kMaxFallVy) vel_.y = kMaxFallVy;

    // 衝突解決
    ResolveHorizontal_();
    ResolveVertical_(dt);

    // 全壊れ床が消滅してたらクリア
    if (!cleared_ && AllFragileGone_()) {
        cleared_ = true;
        finalTime_ = elapsedTime_;
        GoToClearScene_();
        return;
    }
}

// ====== クリア条件チェック ======
bool GameScene::AllFragileGone_() const {
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            Tile t = grid_[y][x];
            if (!IsFragile(t)) continue;
            const FragileState &fs = frag_[y][x];
            if (!fs.gone) {
                return false;
            }
        }
    }
    return true;
}

// ====== クリアシーンへの遷移 ======
void GameScene::GoToClearScene_() {
    if (!sceneManager_) {
        return;
    }

    int nextStage = stageId_ + 1;
    if (nextStage >= maxStageCount_) {
        nextStage = 0;
    }

    engineContext_->sceneManager->ChangeScene(
        std::make_unique<ClearScene>(finalTime_, nextStage)
    );
}

// ====== 壊れ床をarmed状態にする ======
void GameScene::ArmFragile_(int tx, int ty) {
    if (!InMap(tx, ty)) return;
    if (!IsFragile(grid_[ty][tx])) return;

    FragileState &fs = frag_[ty][tx];
    if (fs.gone) return;

    if (!fs.armed) {
        fs.armed = true;

        // ←これ以降は何もしない
        // fs.t をいじらないので、
        // 0秒スタート→fs.t += dtで積算→
        // fs.t > kFragileBreakTime(今2.5f) のタイミングで消える
        //
        // つまり必ずフルの kFragileBreakTime 生きる
        // (= ちゃんと2.5秒生存する)
    }
}

// ====== 背景＋マップ＋プレイヤー描画 ======
void GameScene::DrawBackgroundAndStage_() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *renderer = renderContext_->modelRenderer;

    const float worldW = kMapW * kTile;
    const float worldH = kMapH * kTile;

    auto Deg = [](float d) { return DirectX::XMConvertToRadians(d); };

    auto DrawM = [&](Model &m,
        const DirectX::XMFLOAT3 &p,
        const DirectX::XMFLOAT3 &s,
        const DirectX::XMFLOAT3 &r,
        float alphaMul = 1.0f) {
            Transform t{};
            t.pos = p;
            t.scale = s;
            t.rot = {Deg(r.x), Deg(r.y), Deg(r.z)};
            renderer->Draw(cmd, m, t, alphaMul);
        };

    auto Hash01 = [](int n) {
        uint32_t h = (uint32_t)(n * 2654435761u) ^ 0x9e3779b9u;
        h ^= (h >> 13); h *= 0x5bd1e995u; h ^= (h >> 15);
        return (h & 0xFFFF) / 65535.0f;
        };

    // ===== 1) 遠景演出（夜景・ビル・クレーンなど） =====

    // 夜空ベース
    {
        DrawM(mdlSolid_,
            {0.0f, worldH * 0.50f, 38.0f},
            {worldW * 2.8f, worldH * 2.2f, 0.25f},
            {0.0f, 0.0f, -4.5f});

        for (int i = -3; i <= 3; ++i) {
            float off = i * worldW * 0.25f;
            DrawM(mdlSolid_,
                {off, worldH * (0.6f + 0.08f * std::sin(i * 1.2f)), 37.6f},
                {worldW * 0.9f, worldH * 0.3f, 0.05f},
                {0, 0, (i % 2 == 0) ? -10.0f : 8.0f});
        }

        // 背景の小物
        DrawM(
            switchOn_ ? mdlSwitchOn_ : mdlSwitchOff_,
            {worldW * 0.35f, worldH * 0.85f, 37.0f},
            {1.4f, 1.4f, 0.2f},
            {0, 0, 0}
        );
    }

    auto DrawBuildings = [&](float z, float yBase, float span,
        float wMin, float wMax,
        float hMin, float hMax,
        float tiltDeg) {
            int count = int(worldW / span) + 6;
            for (int i = -count / 2; i <= count / 2; i++) {
                float rx = i * span;
                float rw = wMin + (wMax - wMin) * Hash01(i * 31 + int(z * 10));
                float rh = hMin + (hMax - hMin) * Hash01(i * 97 + int(z * 20));

                // 本体
                DrawM(mdlSolid_,
                    {rx, yBase + rh * 0.5f, z},
                    {rw, rh * 0.5f, 0.22f},
                    {0, 0, ((i & 1) ? tiltDeg : -tiltDeg)});

                // 屋上の箱
                DrawM(mdlSwitchBlockOff_,
                    {rx + rw * 0.15f, yBase + rh + 0.10f, z - 0.05f},
                    {rw * 0.12f, rw * 0.12f, 0.18f},
                    {0, 0, (i & 1) ? -6.0f : 6.0f});

                // 警告灯
                if ((i + (int)z) % 4 == 0) {
                    DrawM(mdlSwitchBlockOn_,
                        {rx, yBase + rh + 0.25f, z - 0.06f},
                        {0.10f, 0.10f, 0.15f},
                        {0, 0, 0});
                }
            }
        };

    DrawBuildings(33.0f, worldH * 0.06f,
        worldW * 0.14f,
        worldW * 0.06f, worldW * 0.10f,
        worldH * 0.14f, worldH * 0.28f,
        2.0f);

    DrawBuildings(30.0f, worldH * 0.08f,
        worldW * 0.12f,
        worldW * 0.07f, worldW * 0.12f,
        worldH * 0.18f, worldH * 0.34f,
        3.0f);

    DrawBuildings(27.0f, worldH * 0.10f,
        worldW * 0.10f,
        worldW * 0.08f, worldW * 0.14f,
        worldH * 0.22f, worldH * 0.40f,
        4.0f);

    // クレーン演出
    {
        // 支柱
        DrawM(mdlJumpOnly_,
            {-worldW * 0.30f, worldH * 0.86f, 24.8f},
            {0.06f, worldH * 0.55f, 0.30f},
            {0,0,0});

        // アーム
        DrawM(mdlSolid_,
            {-worldW * 0.05f, worldH * 1.05f, 24.6f},
            {worldW * 0.55f, 0.06f, 0.30f},
            {0,0,-9.0f});

        // 縦フレーム
        DrawM(mdlSolid_,
            {worldW * 0.22f, worldH * 0.88f, 24.5f},
            {0.035f, worldH * 0.28f, 0.25f},
            {0,0,0});

        // クレーンの操作盤っぽいものをスイッチモデルで
        DrawM(
            switchOn_ ? mdlSwitchOn_ : mdlSwitchOff_,
            {worldW * 0.22f, worldH * 0.72f, 24.4f},
            {0.14f, 0.14f, 0.22f},
            {0,0,0});

        // 吊られてる鉄骨
        DrawM(mdlSolid_,
            {worldW * 0.22f, worldH * 0.55f, 24.3f},
            {0.35f, 0.08f, 0.25f},
            {0,0,4.0f});

        // 投光器
        DrawM(mdlSwitchOn_,
            {worldW * 0.22f, worldH * 0.47f, 24.2f},
            {0.15f, 0.08f, 0.22f},
            {0,0,0});
    }

    auto Flood = [&](DirectX::XMFLOAT3 b, float rotZ, bool blink) {
        // ポール
        DrawM(mdlSolid_,
            {b.x, b.y, 22.0f},
            {0.05f, 0.55f, 0.25f},
            {0,0,0});

        // ヘッド
        DrawM(mdlSwitchOn_,
            {b.x, b.y + 0.38f, 21.9f},
            {0.22f, 0.12f, 0.22f},
            {0,0,rotZ});

        // 下部のスイッチっぽいとこ
        if (blink) {
            DrawM(
                switchOn_ ? mdlSwitchOn_ : mdlSwitchOff_,
                {b.x, b.y - 0.45f, 21.8f},
                {0.12f, 0.12f, 0.15f},
                {0,0,0});
        }
        };

    Flood({-worldW * 0.48f, worldH * 0.82f, 0}, 10.0f, true);
    Flood({worldW * 0.52f, worldH * 0.74f, 0}, 18.0f, false);

    // ===== 2) タイル群 =====
    for (int ty = 0; ty < kMapH; ++ty) {
        for (int tx = 0; tx < kMapW; ++tx) {
            Tile t = grid_[ty][tx];

            // 消えてる壊れ床は描画しない
            if (IsFragile(t) && frag_[ty][tx].gone) {
                continue;
            }

            Model *m = nullptr;
            bool   isFrag = false;
            float  alphaMul = 1.0f;

            switch (t) {
            case Tile::Solid:
                m = &mdlSolid_;
                break;

            case Tile::FragileAny:
                m = &mdlFragileAny_;
                isFrag = true;
                break;

            case Tile::FragileTop:
                m = &mdlFragileTop_;
                isFrag = true;
                break;

            case Tile::FragileBottom:
                m = &mdlFragileBottom_;
                isFrag = true;
                break;

            case Tile::Regen:
                m = &mdlRegen_;
                isFrag = true;
                break;

            case Tile::Spring:
                m = &mdlSpring_;
                break;

            case Tile::Spike:
                m = &mdlSpike_;
                break;

            case Tile::Switch:
                // スイッチ本体（押しボタン）
                m = switchOn_ ? &mdlSwitchOn_ : &mdlSwitchOff_;
                break;

            case Tile::SwitchBlockOn: {
                // ON状態で実体化する床
                m = &mdlSwitchBlockOn_;
                if (switchOn_) {
                    alphaMul = 1.0f;   // 使える側は不透明
                } else {
                    alphaMul = 0.3f;   // 使えない側は半透明ゴースト
                }
            } break;

            case Tile::SwitchBlockOff: {
                // OFF状態で実体化する床
                m = &mdlSwitchBlockOff_;
                if (!switchOn_) {
                    alphaMul = 1.0f;
                } else {
                    alphaMul = 0.3f;
                }
            } break;

            case Tile::JumpOnly:
                m = &mdlJumpOnly_;
                break;

            default:
                break;
            }

            if (!m) continue;

            float wx = xOffset_ + tx * kTile;
            float wy = TyToWorldY(ty);

            // Fragileは点滅アルファを掛け合わせ
            if (isFrag) {
                alphaMul *= FragileBlinkFactor_(tx, ty);
            }

            Transform base{};
            base.pos = {wx + 0.5f * kTile, wy + 0.5f * kTile, 0.0f};
            base.scale = {0.5f, 0.5f, 0.5f * kBlockDepth};
            base.rot = {0,0,0};

            // タイルごとのちょいガタつき
            {
                uint32_t h = (uint32_t)(tx * 73856093u) ^ (uint32_t)(ty * 19349663u);
                h ^= (h >> 13); h *= 0x5bd1e995u;
                float r0 = (float)((h) & 0xFF) / 255.0f;
                float r1 = (float)((h >> 8) & 0xFF) / 255.0f;
                float r2 = (float)((h >> 16) & 0xFF) / 255.0f;

                base.rot.z = Deg((r0 * 2.0f - 1.0f) * 4.0f); // ±4°
                base.scale.x *= (1.0f + (r1 * 0.1f - 0.05f));  // ±5%
                base.scale.y *= (1.0f + (r2 * 0.1f - 0.05f));
            }

            renderer->Draw(cmd, *m, base, alphaMul);
        }
    }

    // ===== 3) プレイヤー =====
    {
        float s = 0.5f;
        DirectX::XMFLOAT3 mn = playerModel_.GetLocalMin();

        Transform p{};
        p.pos = {
            playerTr_.pos.x + pw_ * 0.5f,
            playerTr_.pos.y - (mn.y * s),
            kPlayerZ
        };
        p.scale = {
            s * (pw_ / 1.0f),
            s * (ph_ / 1.0f),
            s * kPlayerDepth
        };
        p.rot = {0,0,0};

        renderer->Draw(cmd, playerModel_, p);
    }

    // ===== 4) 手前フレーム(柵とか) =====
    {
        float y = -0.5f * kTile;

        Transform rail{};
        rail.pos = {0.0f, y, -0.40f};
        rail.scale = {worldW * 0.65f, 0.05f, 0.22f};
        rail.rot = {0,0,0};
        renderer->Draw(cmd, mdlSolid_, rail);

        for (int i = -3; i <= 3; ++i) {
            float x = i * (worldW * 0.16f);

            Transform t{};
            t.pos = {x, y + 0.2f, -0.41f};
            t.scale = {worldW * 0.09f, 0.03f, 0.22f};
            t.rot = {0,0,Deg((i % 2 == 0) ? -10.0f : 12.0f)};
            renderer->Draw(cmd, mdlSolid_, t);
        }

        for (int i = -2; i <= 2; ++i) {
            Transform c{};
            c.pos = {i * (worldW * 0.18f), y + 0.5f, -0.42f};
            c.scale = {worldW * 0.08f, 0.01f, 0.2f};
            c.rot = {0,0,Deg(10.0f * std::sinf(static_cast<float>(i)))};
            renderer->Draw(cmd, mdlJumpOnly_, c);
        }
    }
}


// ====== Draw ======
void GameScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *renderer = renderContext_->modelRenderer;

    renderer->Begin(cmd, dx, camera_);
    DrawBackgroundAndStage_();
    renderer->End(cmd);

    // HUDは今なし
}

// ====== Finalize ======
void GameScene::Finalize() {
    // ComPtrが自動解放するので特に無し
}
