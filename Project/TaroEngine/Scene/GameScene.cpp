#define NOMINMAX
#include "GameScene.h"
#include "imgui/imgui.h"
#include "Input.h"
#include "ModelRenderer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstring>

// ===== 追加インクルード（テクスチャ関連）=====
#include "BufferUtility.h"  // Uploadバッファ作成に使用
#include <DirectXTex/d3dx12.h>
#include <DirectXTex/DirectXTex.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace {
    // 見た目の厚み（Z 方向）※当たり判定は 2D
    constexpr float kBlockDepth = 0.5f;
    constexpr float kPlayerDepth = 0.6f;
    constexpr float kPlayerZ = -0.26f; // -Z が手前
}

// ===== 小ユーティリティ =====
std::wstring GameScene::Widen_(const std::string &u8) {
    if (u8.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// ===== AABB helpers =====
AABB GameScene::PlayerAabbX_() const {
    float w = std::max(0.001f, pw_ - kSkinX * 2.0f);
    return {playerTr_.pos.x + kSkinX, playerTr_.pos.y, w, ph_};
}

// ===== 入力立ち上がり =====
bool GameScene::KeyPressed_(uint8_t dik) {
    auto *in = engineContext_->input;
    bool now = in->IsKeyPressed(dik);
    bool was = keyPrev_[dik] != 0;
    keyPrev_[dik] = now ? 1 : 0;
    return now && !was;
}

// ===== 属性 =====
bool GameScene::IsFragile(Tile t) {
    return t == Tile::FragileAny
        || t == Tile::FragileTop
        || t == Tile::FragileBottom
        || t == Tile::Regen;
}
bool GameScene::IsSpring(Tile t) {
    return t == Tile::Spring;
}
bool GameScene::IsBlockingAt(int tx, int ty) const {
    if (!InMap(tx, ty)) return false;
    Tile t = grid_[ty][tx];

    // 壊れ床は gone ならブロックじゃない
    if (IsFragile(t)) {
        return !frag_[ty][tx].gone;
    }

    // スイッチブロック
    if (t == Tile::SwitchBlockOn)  return switchOn_;
    if (t == Tile::SwitchBlockOff) return !switchOn_;

    if (t == Tile::JumpOnly)       return true; // 一方向床扱いだけど、プレイヤーの処理で上からだけ通す等をやってる

    return t == Tile::Solid;
}

// ===== マップ =====
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

void GameScene::BuildSample() {
    ResetGrid();

    // 地面
    for (int x = 0; x < kMapW; ++x) grid_[kMapH - 2][x] = Tile::Solid;

    // 壊れる床いろいろ
    for (int x = 3; x <= 8; ++x)    grid_[kMapH - 5][x] = Tile::FragileAny;
    for (int x = 11; x <= 14; ++x)  grid_[kMapH - 7][x] = Tile::FragileTop;
    for (int x = 16; x <= 19; ++x)  grid_[kMapH - 9][x] = Tile::FragileBottom;

    // その他ギミック
    grid_[kMapH - 3][6] = Tile::Spring;
    grid_[kMapH - 6][18] = Tile::Switch;
    grid_[kMapH - 6][20] = Tile::SwitchBlockOn;
    grid_[kMapH - 6][21] = Tile::SwitchBlockOn;
    grid_[kMapH - 6][23] = Tile::SwitchBlockOff;

    for (int x = kMapW - 8; x < kMapW - 2; ++x) grid_[kMapH - 3][x] = Tile::Spike;

    grid_[kMapH - 8][22] = Tile::Regen;
}

// ===== CSV / Snapshot =====
static std::string NowStamp() {
    std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}

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

bool GameScene::LoadCSV(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    ResetGrid();

    std::string line;
    if (!std::getline(ifs, line)) return false;

    // header: "W,H,spawnTx,spawnTy"
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

bool GameScene::CreateSnapshot(const std::string &baseCsvPath) const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path base(baseCsvPath);

    fs::path dir = base.parent_path() / "backups";
    fs::create_directories(dir, ec);

    fs::path dst = dir / (base.stem().string() + "_" + NowStamp() + base.extension().string());
    return SaveCSV(dst.string());
}

void GameScene::ClampSpawnToSafe() {
    int tx = std::clamp(spawnTx_, 0, kMapW - 1);
    int ty = std::clamp(spawnTy_, 0, kMapH - 1);
    if (IsBlockingAt(tx, ty)) {
        // 上方向に逃がす（上が+Y）
        for (int r = ty - 1; r >= 0; --r) {
            if (!IsBlockingAt(tx, r)) { ty = r; break; }
        }
    }
    spawnTx_ = tx;
    spawnTy_ = ty;
}

// ===== 画像→SRV ロード（sRGB） =====
bool GameScene::LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
    ComPtr<ID3D12Resource> &outTex,
    D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle) {
    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();
    if (!device || fileU16.empty()) return false;

    // 1) 画像を読み込む（WIC / sRGB 変換）
    DirectX::ScratchImage img, conv;
    HRESULT hr = DirectX::LoadFromWICFile(fileU16.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, img);
    if (FAILED(hr)) return false;

    const DirectX::TexMetadata &meta = img.GetMetadata();

    DXGI_FORMAT targetFmt = meta.format;
    if (!DirectX::IsCompressed(meta.format) && !DirectX::IsSRGB(meta.format)) {
        // 非圧縮＆Linear の場合は RGBA8_sRGB に揃える
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

    // 2) Default ヒープにテクスチャを確保
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
        &heapDef, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outTex));
    if (FAILED(hr)) return false;

    // 3) アップロードリソース作成 & サブリソースコピー
    UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);

    ComPtr<ID3D12Resource> upload = BufferUtility::CreateUploadBuffer(device, uploadSize);

    // 4) 一時コマンド周り（専用キューで実行→待機）
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
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr,
        IID_PPV_ARGS(&list));

    // 5) UpdateSubresources → COPY_DEST から PS 用へ遷移
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

    // 6) フェンスで完了待機
    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    queue->Signal(fence.Get(), 1);
    if (fence->GetCompletedValue() < 1) {
        fence->SetEventOnCompletion(1, evt);
        WaitForSingleObject(evt, INFINITE);
    }
    CloseHandle(evt);

    // 7) SRV を既存ヒープの指定 index に作成（sRGB）
    ID3D12DescriptorHeap *srvHeap = dx->GetSrvHeap();
    const UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += SIZE_T(inc) * srvIndex;

    D3D12_GPU_DESCRIPTOR_HANDLE gpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(inc) * srvIndex;
    outGpuHandle = gpu;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = targetFmt; // すでに sRGB 化済み
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = texDesc.MipLevels;

    device->CreateShaderResourceView(outTex.Get(), &srv, cpu);
    return true;
}

// ===== Initialize =====
void GameScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
    engineContext_ = engineContext;
    renderContext_ = renderContext;

    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // ===== モデル読み込み =====
    // ここでは各ブロックタイプごとに別OBJを割り当てる想定
    playerModel_.Initialize(device, "Resources/Model/Player/player.obj");

    mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");
    mdlFragileAny_.Initialize(device, "Resources/Model/Block/fragile_any.obj");
    mdlFragileTop_.Initialize(device, "Resources/Model/Block/fragile_top.obj");
    mdlFragileBottom_.Initialize(device, "Resources/Model/Block/fragile_bottom.obj");
    mdlRegen_.Initialize(device, "Resources/Model/Block/regen.obj");
    mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
    mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");
    mdlSwitch_.Initialize(device, "Resources/Model/Block/switch.obj");
    mdlSwitchBlockOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchBlockOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");
    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");

    // ===== 各モデルのアルベドテクスチャをSRV化してバインド =====
    auto setupTex = [&](Model &m, UINT srvIndex,
        ComPtr<ID3D12Resource> &holder) {
            if (!m.GetAlbedoPath().empty()) {
                D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
                if (LoadTextureSRV_(Widen_(m.GetAlbedoPath()), srvIndex, holder, gpu)) {
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
    setupTex(mdlSwitch_, kSrvIndex_Switch, texSwitch_);
    setupTex(mdlSwitchBlockOn_, kSrvIndex_SwitchOn, texSwitchOn_);
    setupTex(mdlSwitchBlockOff_, kSrvIndex_SwitchOff, texSwitchOff_);
    setupTex(mdlJumpOnly_, kSrvIndex_JumpOnly, texJumpOnly_);

    // ===== カメラ設定（LH, +Z前という想定）=====
    camera_.Initialize(
        /*eye*/{0.0f, 5.0f, -50.0f},
        /*pitch,yaw,roll*/{0.0f, 0.0f, 0.0f},
        /*fov*/           60.0f,
        /*aspect*/        (float)dx->GetWidth() / std::max(1u, dx->GetHeight()),
        /*near*/          0.1f,
        /*far*/           1000.0f
    );
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    camera_.LookAt({0.0f, 5.0f, -50.0f}, {0.0f, 2.0f, 0.0f});

    // ===== マップ =====
    const float mapW = kMapW * kTile;
    xOffset_ = -mapW * 0.5f; // 中央寄せ

    if (!LoadCSV("stage.csv")) {
        BuildSample();
        (void)SaveCSV("stage.csv");
    }
    ClampSpawnToSafe();

    // ===== プレイヤ初期化 =====
    playerTr_ = {};
    playerTr_.scale = {1,1,1};
    playerTr_.pos = {
        xOffset_ + spawnTx_ * kTile,
        TyToWorldY(spawnTy_) + 0.5f,
        kPlayerZ
    };
    pw_ = 1.0f;
    ph_ = 1.0f;
    vel_ = {0,0,0};
    onGround_ = false;

    std::memset(keyPrev_, 0, sizeof(keyPrev_));
    coyoteCounter_ = 0;
    jumpBuffer_ = 0;

    editorOn_ = false;
    uiVisible_ = true;
    paletteSel_ = 0;
    hoverTx_ = hoverTy_ = -1;
}

// ===== 画面座標→ワールド(Z=0)→タイル =====
bool GameScene::PickTileUnderMouse_(int &outTx, int &outTy, float *outWx, float *outWy) const {
    // ImGui のマウス座標（クライアント座標）
    ImVec2 mp = ImGui::GetIO().MousePos;

    // ビューポート
    auto *dx = engineContext_->directXCommon;
    float vw = (float)dx->GetWidth();
    float vh = (float)dx->GetHeight();
    if (vw <= 0 || vh <= 0) return false;

    // NDC
    float x = (2.0f * mp.x) / vw - 1.0f;
    float y = -(2.0f * mp.y) / vh + 1.0f; // 画面Y下→上

    XMMATRIX invVP = XMMatrixInverse(nullptr, camera_.GetView() * camera_.GetProj()); // LH: view*proj

    // ニア/ファーをワールドへ
    XMVECTOR pNear = XMVectorSet(x, y, 0.0f, 1.0f);
    XMVECTOR pFar = XMVectorSet(x, y, 1.0f, 1.0f);
    pNear = XMVector3TransformCoord(pNear, invVP);
    pFar = XMVector3TransformCoord(pFar, invVP);

    // レイ: o + t*d と Z=0 平面の交点
    XMVECTOR o = pNear;
    XMVECTOR d = XMVector3Normalize(pFar - pNear);

    float oz = XMVectorGetZ(o);
    float dz = XMVectorGetZ(d);
    if (fabsf(dz) < 1e-6f) return false;
    float t = -oz / dz;
    if (t < 0.0f) return false;

    XMVECTOR hit = o + d * t;
    float wx = XMVectorGetX(hit);
    float wy = XMVectorGetY(hit);

    // タイル化
    int tx = ToTx(wx);
    int ty = ToTy(wy);
    if (!InMap(tx, ty)) return false;

    outTx = tx;
    outTy = ty;
    if (outWx) *outWx = wx;
    if (outWy) *outWy = wy;
    return true;
}

// ===== Update =====
void GameScene::Update(float dt) {
    auto *dx = engineContext_->directXCommon;
    auto *in = engineContext_->input;

    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    // トグル類
    if (KeyPressed_(DIK_F1)) {
        bool was = editorOn_;
        editorOn_ = !editorOn_;
        if (was && !editorOn_) {
            (void)CreateSnapshot("stage.csv"); // OFF に切り替わった瞬間にバックアップ
        }
    }
    if (KeyPressed_(DIK_F5)) {
        (void)SaveCSV("stage.csv");
    }
    if (KeyPressed_(DIK_F9)) {
        if (LoadCSV("stage.csv")) {
            ClampSpawnToSafe();
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
    }

    // ===== エディタON中：ゲーム更新は停止 =====
    if (editorOn_) {
        return;
    }

    // ===== 入力 =====
    int ax = 0;
    if (in->IsKeyPressed(DIK_A)) ax -= 1;
    if (in->IsKeyPressed(DIK_D)) ax += 1;
    if (KeyPressed_(DIK_SPACE))  jumpBuffer_ = kJumpBufferFrames;

    // ===== 速度 =====
    vel_.x = (onGround_ ? kMoveGround : kMoveAir) * (float)ax;
    vel_.y += -kGravity;
    if (vel_.y < kMaxFallVy) vel_.y = kMaxFallVy;

    // ===== X移動 =====
    {
        playerTr_.pos.x += vel_.x;
        AABB a = PlayerAabbX_();

        int t = ToTy(a.y + kSkinY);
        int b = ToTy(a.y + a.h - 1e-4f - kSkinY);
        int l = ToTx(a.x);
        int r = ToTx(a.x + a.w - 1e-4f);

        for (int ty = t; ty <= b; ++ty) {
            for (int tx = l; tx <= r; ++tx) {
                if (!InMap(tx, ty)) continue;
                if (!IsBlockingAt(tx, ty)) continue;

                float bx = xOffset_ + tx * kTile;
                float by = TyToWorldY(ty);
                if (!OverlapXY(a, bx, by, kTile, kTile)) continue;

                if (vel_.x > 0) {
                    playerTr_.pos.x = (bx - pw_) - kSkinX;
                } else if (vel_.x < 0) {
                    playerTr_.pos.x = (bx + kTile) + kSkinX;
                }

                a = PlayerAabbX_();
            }
        }
    }

    // ===== Y移動（上下 + ギミック） =====
    bool switchOverlapNow = false;
    {
        const float yPrev = playerTr_.pos.y;
        playerTr_.pos.y += vel_.y;

        AABB aFull = PlayerAabbFull_();
        int l = ToTx(aFull.x);
        int r = ToTx(aFull.x + aFull.w - 1e-4f);

        onGround_ = false;

        if (vel_.y <= 0.0f) {
            // 下向き移動＝着地チェック
            int rowBelow = ToTy(aFull.y - kSkinY);

            for (int tx = l; tx <= r; ++tx) {
                if (!InMap(tx, rowBelow)) continue;
                Tile tt = grid_[rowBelow][tx];

                float bx = xOffset_ + tx * kTile;
                float by = TyToWorldY(rowBelow);
                float topY = by + kTile;

                // スプリング
                if (IsSpring(tt)) {
                    if (OverlapXY(PlayerAabbFull_(), bx, by, kTile, kTile)) {
                        vel_.y = kSpringVy;
                        onGround_ = false;
                    }
                }

                // スイッチ
                if (tt == Tile::Switch) {
                    if (OverlapXY(PlayerAabbFull_(), bx, by, kTile, kTile)) {
                        switchOverlapNow = true;
                    }
                }

                // 通常の床着地
                if (IsBlockingAt(tx, rowBelow) &&
                    (yPrev - kSkinY) >= topY &&
                    (playerTr_.pos.y - kSkinY) < topY &&
                    aFull.x < bx + kTile &&
                    aFull.x + aFull.w > bx) {
                    // 踏んだ瞬間に壊れ床をarmedにする
                    if (IsFragile(tt) && !frag_[rowBelow][tx].gone) {
                        frag_[rowBelow][tx].armed = true;
                    }

                    playerTr_.pos.y = topY + kSkinY;
                    vel_.y = 0.0f;
                    onGround_ = true;

                    aFull = PlayerAabbFull_();
                    l = ToTx(aFull.x);
                    r = ToTx(aFull.x + aFull.w - 1e-4f);
                    break;
                }
            }
        } else {
            // 上昇中＝頭ぶつけ処理
            int rowAbove = ToTy(aFull.y + aFull.h + kSkinY);
            for (int tx = l; tx <= r; ++tx) {
                if (!InMap(tx, rowAbove)) continue;
                Tile tt = grid_[rowAbove][tx];

                float bx = xOffset_ + tx * kTile;
                float by = TyToWorldY(rowAbove);
                float bottomY = by;

                // 下から当てて壊せる床をarmedにする（FragileBottomやAnyなど）
                if (IsFragile(tt) && !frag_[rowAbove][tx].gone) {
                    const bool canFromBelow = (tt == Tile::FragileAny || tt == Tile::FragileBottom);
                    if (canFromBelow) {
                        float overlapX = std::min(aFull.x + aFull.w, bx + kTile)
                            - std::max(aFull.x, bx);
                        if (overlapX > (4.0f / 48.0f)) {
                            frag_[rowAbove][tx].armed = true;
                        }
                    }
                }

                if (IsBlockingAt(tx, rowAbove) &&
                    (yPrev + ph_ + kSkinY) <= bottomY &&
                    (playerTr_.pos.y + ph_ + kSkinY) > bottomY &&
                    aFull.x < bx + kTile &&
                    aFull.x + aFull.w > bx) {
                    playerTr_.pos.y = bottomY - ph_ - kSkinY;
                    vel_.y = 0.0f;

                    aFull = PlayerAabbFull_();
                    l = ToTx(aFull.x);
                    r = ToTx(aFull.x + aFull.w - 1e-4f);
                    break;
                }
            }
        }

        // プレイヤーの足がほんの少しだけブロックにかかっていても armed にしてあげるやつ
        {
            int l2 = ToTx(playerTr_.pos.x);
            int r2 = ToTx(playerTr_.pos.x + pw_ - 1e-4f);
            int by = ToTy(playerTr_.pos.y - kSkinY);
            const float kMinOverlap = 4.0f / 48.0f;
            for (int tx2 = l2; tx2 <= r2; ++tx2) {
                if (!InMap(tx2, by)) continue;
                Tile tt = grid_[by][tx2];
                if (!IsFragile(tt) || frag_[by][tx2].gone) continue;

                // 上から踏んで壊す対象は FragileAny / FragileTop / Regen
                if (!(tt == Tile::FragileAny || tt == Tile::FragileTop || tt == Tile::Regen)) {
                    continue;
                }

                float bx = xOffset_ + tx2 * kTile;
                float overlapX = std::min(playerTr_.pos.x + pw_, bx + kTile)
                    - std::max(playerTr_.pos.x, bx);
                if (overlapX > kMinOverlap) {
                    frag_[by][tx2].armed = true;
                }
            }
        }
    }

    // スイッチ（接触エッジでトグル）
    static bool prevSw = false;
    if (switchOverlapNow && !prevSw) {
        switchOn_ = !switchOn_;
    }
    prevSw = switchOverlapNow;

    // コヨーテ/ジャンプバッファ
    if (onGround_) coyoteCounter_ = kCoyoteMaxFrames;
    else if (coyoteCounter_ > 0) --coyoteCounter_;

    if (jumpBuffer_ > 0) --jumpBuffer_;

    if ((onGround_ || coyoteCounter_ > 0) && jumpBuffer_ > 0) {
        vel_.y = kJumpVy;
        onGround_ = false;
        jumpBuffer_ = 0;
    }

    // 床へ吸着（Yをタイルグリッドにスナップ）
    if (onGround_) {
        float targetY = std::floor((playerTr_.pos.y - kSkinY) / kTile) * kTile + kSkinY;
        if (std::fabs(playerTr_.pos.y - targetY) > 1e-4f) {
            playerTr_.pos.y = targetY;
        }
    }

    // 壊れ床/復活床進行
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            Tile t = grid_[y][x];
            if (!IsFragile(t)) continue;

            auto &fs = frag_[y][x];
            if (!fs.gone) {
                if (fs.armed) {
                    fs.t += dt;
                    constexpr float kFragileBreakTime = 1.35f;
                    if (fs.t > kFragileBreakTime) {
                        fs.gone = true;
                        if (t == Tile::Regen) {
                            regen_[y][x].respawn = 0.0f;
                        }
                    }
                }
            } else if (t == Tile::Regen) {
                regen_[y][x].respawn += dt;
                constexpr float kRegenRespawnTime = 2.0f;
                if (regen_[y][x].respawn >= kRegenRespawnTime) {
                    fs = FragileState{};
                }
            }
        }
    }

    // 死亡（落下/スパイク）
    {
        float minY = -4.0f * kTile;
        if (playerTr_.pos.y < minY) {
            ClampSpawnToSafe();
            playerTr_.pos = {
                xOffset_ + spawnTx_ * kTile,
                TyToWorldY(spawnTy_) + 0.5f,
                kPlayerZ
            };
            vel_ = {0,0,0};
            onGround_ = false;
            coyoteCounter_ = 0;
            jumpBuffer_ = 0;
        } else {
            AABB f = PlayerAabbFull_();
            int l = ToTx(f.x);
            int r = ToTx(f.x + f.w - 1e-4f);
            int t = ToTy(f.y);
            int b = ToTy(f.y + f.h - 1e-4f);

            for (int ty = t; ty <= b; ++ty) {
                for (int tx = l; tx <= r; ++tx) {
                    if (!InMap(tx, ty)) continue;
                    if (grid_[ty][tx] != Tile::Spike) continue;

                    float bx = xOffset_ + tx * kTile;
                    float by = TyToWorldY(ty);
                    if (OverlapXY(f, bx, by, kTile, kTile)) {
                        ClampSpawnToSafe();
                        playerTr_.pos = {
                            xOffset_ + spawnTx_ * kTile,
                            TyToWorldY(spawnTy_) + 0.5f,
                            kPlayerZ
                        };
                        vel_ = {0,0,0};
                        onGround_ = false;
                        coyoteCounter_ = 0;
                        jumpBuffer_ = 0;

                        ty = b + 1; // 二重リセット防止で外側ループbreak
                        break;
                    }
                }
            }
        }
    }
}

// ===== ImGui エディタ（※Drawからのみ呼ぶ） =====
void GameScene::EditorUI_() {
    ImGuiIO &io = ImGui::GetIO();

    // ==== パレット（Novice 互換）====
    static const char *kNames[] = {
        "Empty","Solid","FragileAny","FragileTop","FragileBottom","Spring","Spike",
        "JumpOnly","Regen","Switch","SwitchBlockOn","SwitchBlockOff","Spawn"
    };
    constexpr int kMaxPalIndex = 12; // 12=Spawn

    // ===== メインウィンドウ =====
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextUnformatted("F1 toggle / F5 save / F9 load");
    ImGui::Checkbox("HUD visible", &uiVisible_);
    ImGui::Separator();

    // パレット
    ImGui::SliderInt("Palette", &paletteSel_, 0, kMaxPalIndex);
    ImGui::SameLine();
    ImGui::Text("[%s]", kNames[std::clamp(paletteSel_, 0, kMaxPalIndex)]);

    // Q/E/ホイール/数値キー
    if (ImGui::IsKeyPressed(ImGuiKey_Q)) paletteSel_ = (paletteSel_ - 1 + (kMaxPalIndex + 1)) % (kMaxPalIndex + 1);
    if (ImGui::IsKeyPressed(ImGuiKey_E)) paletteSel_ = (paletteSel_ + 1) % (kMaxPalIndex + 1);
    if (io.MouseWheel > 0) paletteSel_ = (paletteSel_ + 1) % (kMaxPalIndex + 1);
    if (io.MouseWheel < 0) paletteSel_ = (paletteSel_ - 1 + (kMaxPalIndex + 1)) % (kMaxPalIndex + 1);
    for (int n = 0; n <= 9; ++n) {
        ImGuiKey key = (n == 0) ? ImGuiKey_0 : (ImGuiKey)(ImGuiKey_1 + (n - 1));
        if (ImGui::IsKeyPressed(key)) {
            int idx = (n == 0) ? 9 : (n - 1);
            if (idx <= kMaxPalIndex) paletteSel_ = idx;
        }
    }

    // Spawn 表示
    ImGui::Separator();
    ImGui::Text("Spawn: (%d, %d)", spawnTx_, spawnTy_);
    ImGui::Text("Switch: %s", switchOn_ ? "ON" : "OFF");

    // Save / Load / Snapshot
    if (ImGui::Button("Save CSV (F5)")) {
        (void)SaveCSV("stage.csv");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load CSV (F9)")) {
        if (LoadCSV("stage.csv")) {
            ClampSpawnToSafe();
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
    }
    ImGui::SameLine();
    if (ImGui::Button("Snapshot backups/")) {
        (void)CreateSnapshot("stage.csv");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Mouse: L=place  R=erase  M=pick  (Spawn slot: set spawn; Shift+L: teleport)");
    ImGui::End();

    // ===== マウス→タイル =====
    hoverTx_ = hoverTy_ = -1;
    float hitWx = 0, hitWy = 0;
    bool hit = PickTileUnderMouse_(hoverTx_, hoverTy_, &hitWx, &hitWy);

    // ===== 配置/消し/スポイト（ImGuiがマウスを奪ってない時のみ） =====
    if (hit && !io.WantCaptureMouse) {
        bool left = ImGui::IsMouseDown(0);
        bool right = ImGui::IsMouseDown(1);
        bool middle = ImGui::IsMouseClicked(2); // スポイトは押し始め

        if (middle) {
            // スポイト
            Tile cur = grid_[hoverTy_][hoverTx_];
            int id = (int)cur;
            id = std::clamp(id, 0, 11);
            paletteSel_ = id;
        } else if (left) {
            // 置く or Spawn
            if (paletteSel_ == 12) {
                spawnTx_ = hoverTx_;
                spawnTy_ = hoverTy_;
                ClampSpawnToSafe();

                const bool lshift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
                if (lshift) {
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
            } else {
                Tile t = (Tile)std::clamp(paletteSel_, 0, 11);
                grid_[hoverTy_][hoverTx_] = t;

                if (IsFragile(t)) {
                    frag_[hoverTy_][hoverTx_] = FragileState{};
                }
                if (t == Tile::Regen) {
                    regen_[hoverTy_][hoverTx_] = RegenState{};
                }
            }
        } else if (right) {
            // 消しゴム（Spawn 選択中は何もしない）
            if (paletteSel_ != 12) {
                grid_[hoverTy_][hoverTx_] = Tile::Empty;
                frag_[hoverTy_][hoverTx_] = FragileState{};
                regen_[hoverTy_][hoverTx_] = RegenState{};
            }
        }
    }

    // ===== 画面オーバーレイ（グリッド・ホバー・Spawn 旗） =====
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    auto *dx = engineContext_->directXCommon;
    const float W = (float)dx->GetWidth();
    const float H = (float)dx->GetHeight();

    auto WorldToScreen = [&](float wx, float wy, ImVec2 &out) -> bool {
        XMVECTOR p = XMVectorSet(wx, wy, 0.0f, 1.0f);
        XMVECTOR clip = XMVector4Transform(p, camera_.GetView() * camera_.GetProj());
        float w = XMVectorGetW(clip);
        if (w <= 0.0f) return false;
        XMVECTOR ndc = clip / w;
        float nx = XMVectorGetX(ndc);
        float ny = XMVectorGetY(ndc);
        out.x = (nx * 0.5f + 0.5f) * W;
        out.y = (-ny * 0.5f + 0.5f) * H;
        return true;
        };

    // ホバー枠
    if (hoverTx_ >= 0 && hoverTy_ >= 0) {
        float bx = xOffset_ + hoverTx_ * kTile;
        float by = TyToWorldY(hoverTy_);
        ImVec2 s0, s1, s2, s3;
        if (WorldToScreen(bx, by, s0) &&
            WorldToScreen(bx + kTile, by, s1) &&
            WorldToScreen(bx + kTile, by + kTile, s2) &&
            WorldToScreen(bx, by + kTile, s3)) {
            ImVec2 poly[4] = {s0,s1,s2,s3};
            dl->AddPolyline(poly, 4, IM_COL32(255, 255, 255, 140), true, 2.0f);
        }
    }

    // Spawn 旗枠
    {
        float bx = xOffset_ + spawnTx_ * kTile;
        float by = TyToWorldY(spawnTy_);
        ImVec2 s0, s1, s2, s3;
        if (WorldToScreen(bx, by, s0) &&
            WorldToScreen(bx + kTile, by, s1) &&
            WorldToScreen(bx + kTile, by + kTile, s2) &&
            WorldToScreen(bx, by + kTile, s3)) {
            ImVec2 poly[4] = {s0,s1,s2,s3};
            dl->AddPolyline(poly, 4, IM_COL32(160, 200, 255, 120), true, 1.5f);
        }
    }
}

void GameScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();

    auto *renderer = renderContext_->modelRenderer;

    // 修正後のBegin呼び出し
    renderer->Begin(cmd, dx, camera_);

    // ---- タイル描画 ----
    // （ここから先はあなたの元の処理でOK。Zバイアスはまだ入れなくていい）
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            Tile t = grid_[y][x];

            if (IsFragile(t) && frag_[y][x].gone && t != Tile::Regen) {
                continue;
            }
            if (t == Tile::SwitchBlockOn && !switchOn_) continue;
            if (t == Tile::SwitchBlockOff && switchOn_) continue;

            Model *drawModel = nullptr;
            switch (t) {
            case Tile::Solid:              drawModel = &mdlSolid_; break;
            case Tile::FragileAny:         drawModel = &mdlFragileAny_; break;
            case Tile::FragileTop:         drawModel = &mdlFragileTop_; break;
            case Tile::FragileBottom:      drawModel = &mdlFragileBottom_; break;
            case Tile::Regen:              drawModel = &mdlRegen_; break;
            case Tile::Spring:             drawModel = &mdlSpring_; break;
            case Tile::Spike:              drawModel = &mdlSpike_; break;
            case Tile::Switch:             drawModel = &mdlSwitch_; break;
            case Tile::SwitchBlockOn:      drawModel = &mdlSwitchBlockOn_; break;
            case Tile::SwitchBlockOff:     drawModel = &mdlSwitchBlockOff_; break;
            case Tile::JumpOnly:           drawModel = &mdlJumpOnly_; break;
            default:
                break;
            }
            if (!drawModel) continue;

            const float bx = xOffset_ + x * kTile;
            const float by = TyToWorldY(y);

            Transform tr{};
            tr.pos = {
                bx + kTile * 0.5f,
                by + kTile * 0.5f,
                0.0f
            };
            tr.scale = {0.5f, 0.5f, 0.5f * kBlockDepth};

            renderer->Draw(cmd, *drawModel, tr);
        }
    }

    // ---- プレイヤー ----
    {
        Transform drawTr = playerTr_;
        drawTr.pos = {
            playerTr_.pos.x + pw_ * 0.5f,
            playerTr_.pos.y + ph_ * 0.5f,
            kPlayerZ
        };
        drawTr.scale = {0.5f, 0.5f, 0.5f * kPlayerDepth};
        renderer->Draw(cmd, playerModel_, drawTr);
    }

    renderer->End(cmd);

    // ====== ImGui ======
    if (editorOn_) {
        EditorUI_();
    } else if (uiVisible_) {
        ImGui::Begin("HUD", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("F1: Editor  F5: Save  F9: Load");
        ImGui::Text("Switch: %s", switchOn_ ? "ON" : "OFF");
        ImGui::End();
    }
}

void GameScene::Finalize() {
    // 今のところ特に解放は不要
}
