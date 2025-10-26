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

#include "BufferUtility.h"
#include <DirectXTex/d3dx12.h>
#include <DirectXTex/DirectXTex.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace {
    constexpr float kBlockDepth = 0.5f;
    constexpr float kPlayerDepth = 0.6f;
    constexpr float kPlayerZ = -0.26f; // -Z が手前
}

// ====== 文字コードユーティリティ ======
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

// 「ブロックとしてぶつかるか？」
bool GameScene::IsBlockingAt(int tx, int ty) const {
    if (!InMap(tx, ty)) return false;
    Tile t = grid_[ty][tx];

    if (IsFragile(t)) {                     // fragile
        if (frag_[ty][tx].gone) return false;
        return true;
    }
    if (t == Tile::SwitchBlockOn)  return switchOn_;
    if (t == Tile::SwitchBlockOff) return !switchOn_;
    if (t == Tile::JumpOnly)       return true;   // JumpOnly 足場
    if (t == Tile::Solid)          return true;   // Solid
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

    // 壊れる床
    for (int x = 3; x <= 8; ++x)  grid_[kMapH - 5][x] = Tile::FragileAny;
    for (int x = 11; x <= 14; ++x) grid_[kMapH - 7][x] = Tile::FragileTop;
    for (int x = 16; x <= 19; ++x) grid_[kMapH - 9][x] = Tile::FragileBottom;

    // ギミック
    grid_[kMapH - 3][6] = Tile::Spring;
    grid_[kMapH - 6][18] = Tile::Switch;
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

// ====== CSV保存・読み込み ======
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
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
        lt.tm_hour, lt.tm_min, lt.tm_sec);
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
        for (int r = ty - 1; r >= 0; --r) { // 上方向に安全地帯を探す
            if (!IsBlockingAt(tx, r)) { ty = r; break; }
        }
    }
    spawnTx_ = tx;
    spawnTy_ = ty;
}

// ====== テクスチャ読み込み + SRV作成 ======
bool GameScene::LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
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

    // ワンショットコマンド
    ComPtr<ID3D12CommandQueue> queue;
    {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    }
    ComPtr<ID3D12CommandAllocator> alloc;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));

    ComPtr<ID3D12GraphicsCommandList> list;
    device->CreateCommandList(0,
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

    // SRV
    ID3D12DescriptorHeap *srvHeap = dx->GetSrvHeap();
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

    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // モデルロード
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

    // テクスチャ割り当て
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
    setupTex(mdlSwitch_, kSrvIndex_Switch, texSwitch_);
    setupTex(mdlSwitchBlockOn_, kSrvIndex_SwitchOn, texSwitchOn_);
    setupTex(mdlSwitchBlockOff_, kSrvIndex_SwitchOff, texSwitchOff_);
    setupTex(mdlJumpOnly_, kSrvIndex_JumpOnly, texJumpOnly_);

    // マップオフセット（中央寄せ）
    const float mapW = kMapW * kTile;
    xOffset_ = -mapW * 0.5f;

    // CSVロード or サンプル構築
    if (!LoadCSV("stage.csv")) {
        BuildSample();
        (void)SaveCSV("stage.csv");
    }
    ClampSpawnToSafe();

    // プレイヤ初期
    playerTr_ = {};
    playerTr_.scale = {1,1,1};
    playerTr_.pos = {
        xOffset_ + spawnTx_ * kTile,
        TyToWorldY(spawnTy_) + 0.5f,
        kPlayerZ
    };

    vel_ = {0,0,0};
    onGround_ = false;

    std::memset(keyPrev_, 0, sizeof(keyPrev_));
    coyoteCounter_ = 0;
    jumpBuffer_ = 0;

    editorOn_ = false;
    uiVisible_ = true;
    paletteSel_ = 0;
    hoverTx_ = -1;
    hoverTy_ = -1;

    // カメラ（マップ全体を正射影で見る）
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

// ====== 画面マウス→タイル ======
bool GameScene::PickTileUnderMouse_(int &outTx, int &outTy, float *outWx, float *outWy) const {
    ImVec2 mp = ImGui::GetIO().MousePos;

    auto *dx = engineContext_->directXCommon;
    float vw = (float)dx->GetWidth();
    float vh = (float)dx->GetHeight();
    if (vw <= 0 || vh <= 0) return false;

    // NDC
    float x = (2.0f * mp.x) / vw - 1.0f;
    float y = -(2.0f * mp.y) / vh + 1.0f;

    XMMATRIX invVP = XMMatrixInverse(nullptr, camera_.GetView() * camera_.GetProj());

    XMVECTOR pNear = XMVectorSet(x, y, 0.0f, 1.0f);
    XMVECTOR pFar = XMVectorSet(x, y, 1.0f, 1.0f);
    pNear = XMVector3TransformCoord(pNear, invVP);
    pFar = XMVector3TransformCoord(pFar, invVP);

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

    int tx = ToTx(wx);
    int ty = ToTy(wy);
    if (!InMap(tx, ty)) return false;

    outTx = tx;
    outTy = ty;
    if (outWx) *outWx = wx;
    if (outWy) *outWy = wy;
    return true;
}

// ====== スイープ式 横移動解決 ======
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
        // 右方向
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
        // 左方向
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

// ====== スイープ式 縦移動解決＋ギミック/ジャンプ制御 ======
void GameScene::ResolveVertical_(float dt) {
    float startY = playerTr_.pos.y;
    float targetY = startY + vel_.y;

    // AABBの現在形
    AABB boxNow = PlayerAabbFull_();

    // X方向の列
    float minX = boxNow.x;
    float maxX = boxNow.x + boxNow.w - 1e-4f;
    int txL = ToTx(minX);
    int txR = ToTx(maxX);
    int txMin = std::min(txL, txR);
    int txMax = std::max(txL, txR);

    onGround_ = false;
    bool switchOverlapNow = false;

    if (vel_.y <= 0.0f) {
        // ====== 落下・着地 ======
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
                float topY = by + kTile; // タイル上面

                // スプリング / スイッチ重なり（着地後想定位置）
                {
                    AABB afterBox{boxNow.x, targetY, boxNow.w, boxNow.h};
                    if (IsSpring(tt)) {
                        if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
                            vel_.y = kSpringVy;
                        }
                    }
                    if (tt == Tile::Switch) {
                        if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
                            switchOverlapNow = true;
                        }
                    }
                }

                if (!IsBlockingAt(tx, row)) continue;

                // 下降で床にヒットした？
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
                                frag_[row][tx].armed = true;
                            }
                        }
                    }
                }
            }
        }

        if (hitFloor) {
            playerTr_.pos.y = bestSnapY;
            vel_.y = 0.0f;
            onGround_ = true;
        } else {
            playerTr_.pos.y = targetY;
        }
    } else {
        // ====== 上昇・頭ぶつけ ======
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
                float bottomY = by; // タイル下面

                // 下から壊せるfragileへのヒビ付与
                if (IsFragile(tt) && !frag_[row][tx].gone) {
                    bool canFromBelow = (tt == Tile::FragileAny || tt == Tile::FragileBottom);
                    if (canFromBelow) {
                        float overlapX =
                            std::min(boxNow.x + boxNow.w, bx + kTile)
                            - std::max(boxNow.x, bx);
                        if (overlapX > kMinGroundOverlap) {
                            frag_[row][tx].armed = true;
                        }
                    }
                }

                if (!IsBlockingAt(tx, row)) continue;

                // 上昇で天井にヒットした？
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

    // 足元微接地でfragile armed化
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

            if (!(tt == Tile::FragileAny || tt == Tile::FragileTop || tt == Tile::Regen)) continue;

            float bx = xOffset_ + tx * kTile;
            float overlapX =
                std::min(playerTr_.pos.x + pw_, bx + kTile)
                - std::max(playerTr_.pos.x, bx);
            if (overlapX > kMinGroundOverlap) {
                frag_[rowBelow][tx].armed = true;
            }
        }
    }

    // スイッチトグル
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

    // 接地時の床スナップ
    if (onGround_) {
        float stableY = std::floor((playerTr_.pos.y - kSkinY) / kTile) * kTile + kSkinY;
        if (std::fabs(playerTr_.pos.y - stableY) > 1e-4f) {
            playerTr_.pos.y = stableY;
        }
    }

    // fragile / regen タイマー
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

    // デス判定
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
}

// ====== Update ======
void GameScene::Update(float dt) {
    auto *dx = engineContext_->directXCommon;
    auto *in = engineContext_->input;

    // ウィンドウリサイズに合わせてカメラのOrthoサイズ調整
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

    // エディタ系トグル
    if (KeyPressed_(DIK_F1)) {
        bool was = editorOn_;
        editorOn_ = !editorOn_;
        if (was && !editorOn_) {
            (void)CreateSnapshot("stage.csv"); // エディタ終了時にバックアップ
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

    // エディタ中は物理止める
    if (editorOn_) return;

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

    // 物理解決（軸分離スイープ）
    ResolveHorizontal_();
    ResolveVertical_(dt);
}

// ====== 3D 背景（ImGui を使わない版） ======
void GameScene::DrawBackground3D_(ModelRenderer *mr) {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();

    const float worldW = kMapW * kTile;
    const float worldH = kMapH * kTile;

    auto Deg = [](float d) { return XMConvertToRadians(d); };
    auto DrawM = [&](Model &m, XMFLOAT3 p, XMFLOAT3 s, XMFLOAT3 r) {
        Transform t{}; t.pos = p; t.scale = s; t.rot = {Deg(r.x),Deg(r.y),Deg(r.z)};
        mr->Draw(cmd, m, t);
        };

    // === 奥：夜空（巨大板） Z=+30
    DrawM(mdlSolid_, {0.0f, worldH * 0.5f, 30.0f},
        {worldW * 2.5f, worldH * 2.5f, 0.2f},
        {0,0,-1.0f});

    // === ビル群（シルエット） Z=+28 付近
    for (int i = -7; i <= 7; i++) {
        float x = i * (worldW * 0.12f);
        float r = 0.5f + 0.5f * std::sin(i * 7.3f);
        float w = worldW * (0.08f + 0.04f * r);
        float h = worldH * (0.18f + 0.22f * r);
        DrawM(mdlSolid_, {x, h * 0.5f + worldH * 0.05f, 28.0f}, {w, h * 0.5f, 0.2f}, {0,0,0});
        DrawM(mdlSolid_, {x, h + worldH * 0.06f, 27.8f}, {0.08f, 0.08f, 0.08f}, {0,0,0});
    }

    // === クレーンアーム / ワイヤ Z=+26
    DrawM(mdlSolid_, {worldW * 0.10f, worldH * 0.92f, 26.0f}, {worldW * 0.50f, 0.06f, 0.30f}, {0,0,-8.0f});
    DrawM(mdlSolid_, {worldW * 0.36f, worldH * 0.72f, 26.0f}, {0.035f,     worldH * 0.28f, 0.25f}, {0,0,0});
    DrawM(mdlSolid_, {worldW * 0.36f, worldH * 0.55f, 25.8f}, {0.10f, 0.10f, 0.25f}, {0,0,0});

    // === 奥の手すり・投光器 Z=+12〜+9
    auto Beam = [&](XMFLOAT3 p, XMFLOAT3 s, float rz) {
        DrawM(mdlSolid_, p, s, {0,0,rz});
        };
    // 縦柱
    for (int i = 0; i < 4; i++) {
        float rx = (i / 3.0f - 0.5f) * worldW * 0.95f;
        Beam({rx, worldH * 0.65f, 10.0f}, {0.06f, worldH * 0.42f, 0.28f}, 0);
    }
    // 横梁
    Beam({0.0f,           worldH * 0.86f, 10.0f}, {worldW * 0.55f, 0.04f, 0.28f}, 0);
    Beam({0.0f,           worldH * 0.46f, 10.0f}, {worldW * 0.55f, 0.04f, 0.28f}, 0);
    Beam({-worldW * 0.42f,  worldH * 0.52f,  9.6f}, {worldW * 0.50f, 0.03f, 0.28f}, 35);
    Beam({worldW * 0.42f,  worldH * 0.52f,  9.6f}, {worldW * 0.50f, 0.03f, 0.28f}, -35);

    auto Flood = [&](XMFLOAT3 b) {
        DrawM(mdlSolid_, {b.x, b.y,         9.8f}, {0.05f, 0.55f, 0.22f}, {0,0,0});
        DrawM(mdlSolid_, {b.x, b.y + 0.38f, 9.6f}, {0.20f, 0.10f, 0.22f}, {0,0, 6});
        DrawM(mdlSolid_, {b.x, b.y + 0.38f, 9.5f}, {0.24f, 0.13f, 0.22f}, {0,0, 6});
        };
    Flood({-worldW * 0.48f, worldH * 0.76f, 0});
    Flood({worldW * 0.48f, worldH * 0.66f, 0});
}

// ====== EditorUI（ImGui でのエディタ。背景は描かない） ======
void GameScene::EditorUI_() {
    ImGuiIO &io = ImGui::GetIO();

    static const char *kNames[] = {
        "Empty","Solid","FragileAny","FragileTop","FragileBottom",
        "Spring","Spike","JumpOnly","Regen","Switch",
        "SwitchBlockOn","SwitchBlockOff","Spawn"
    };
    constexpr int kMaxPalIndex = 12;

    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextUnformatted("F1 toggle / F5 save / F9 load");
    ImGui::Checkbox("HUD visible", &uiVisible_);
    ImGui::Separator();

    // パレット選択
    ImGui::SliderInt("Palette", &paletteSel_, 0, kMaxPalIndex);
    ImGui::SameLine();
    ImGui::Text("[%s]", kNames[std::clamp(paletteSel_, 0, kMaxPalIndex)]);

    // Q/E, ホイール, 数字キーで選択
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

    ImGui::Separator();
    ImGui::Text("Spawn: (%d, %d)", spawnTx_, spawnTy_);
    ImGui::Text("Switch: %s", switchOn_ ? "ON" : "OFF");

    if (ImGui::Button("Save CSV (F5)")) { (void)SaveCSV("stage.csv"); }
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
    if (ImGui::Button("Snapshot backups/")) { (void)CreateSnapshot("stage.csv"); }

    ImGui::Separator();
    ImGui::TextUnformatted("Mouse: L=place  R=erase  M=pick");
    ImGui::TextUnformatted("(Spawn slot: set spawn; Shift+L: teleport)");
    ImGui::End();

    // ===== タイル編集 =====
    hoverTx_ = hoverTy_ = -1;
    float hitWx = 0, hitWy = 0;
    bool hit = PickTileUnderMouse_(hoverTx_, hoverTy_, &hitWx, &hitWy);

    if (hit && !io.WantCaptureMouse) {
        bool left = ImGui::IsMouseDown(0);
        bool right = ImGui::IsMouseDown(1);
        bool middle = ImGui::IsMouseClicked(2);

        if (middle) {
            Tile cur = grid_[hoverTy_][hoverTx_];
            int id = (int)cur;
            id = std::clamp(id, 0, 11);
            paletteSel_ = id;
        } else if (left) {
            if (paletteSel_ == 12) {
                // spawn
                spawnTx_ = hoverTx_;
                spawnTy_ = hoverTy_;
                ClampSpawnToSafe();

                bool lshift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
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
            if (paletteSel_ != 12) {
                grid_[hoverTy_][hoverTx_] = Tile::Empty;
                frag_[hoverTy_][hoverTx_] = FragileState{};
                regen_[hoverTy_][hoverTx_] = RegenState{};
            }
        }
    }

    // ===== 画面上のハイライト（ImGuiの前景） =====
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    auto *dx2 = engineContext_->directXCommon;
    const float W = (float)dx2->GetWidth();
    const float H = (float)dx2->GetHeight();

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

    // ホバー中タイル枠
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

    // Spawn位置枠
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

// ====== Draw ======
void GameScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *renderer = renderContext_->modelRenderer;

    renderer->Begin(cmd, dx, camera_);

    const float worldW = kMapW * kTile;
    const float worldH = kMapH * kTile;

    auto Deg = [](float d) { return XMConvertToRadians(d); };
    auto DrawM = [&](Model &m, const XMFLOAT3 &p, const XMFLOAT3 &s, const XMFLOAT3 &r) {
        Transform t{}; t.pos = p; t.scale = s; t.rot = {Deg(r.x),Deg(r.y),Deg(r.z)};
        renderer->Draw(cmd, m, t);
        };
    auto Hash01 = [](int n) {
        // 0..1 の乱数っぽい値
        uint32_t h = (uint32_t)(n * 2654435761u) ^ 0x9e3779b9u;
        h ^= (h >> 13); h *= 0x5bd1e995u; h ^= (h >> 15);
        return (h & 0xFFFF) / 65535.0f;
        };

    // ========= ここから“3D背景” =========
    // 1) 空：巨大プレーン（ごく薄い傾きで単調さを軽減）
    {
        DrawM(mdlSolid_,
            {0.0f, worldH * 0.50f, 38.0f},
            {worldW * 2.6f, worldH * 2.0f, 0.25f},
            {0.0f, 0.0f, -3.5f});
        // うっすら層をもう1枚（色は同じでも法線/UV変化でテクスチャ差が出やすい）
        DrawM(mdlSolid_,
            {0.0f, worldH * 0.55f, 37.2f},
            {worldW * 2.2f, worldH * 1.8f, 0.20f},
            {0.0f, 0.0f, 8.0f});
    }

    // 2) 超遠景のビル群（シルエット）— 奥から手前へ3レイヤ
    auto DrawBuildings = [&](float z, float yBase, float span, float wMin, float wMax, float hMinH, float hMaxH, float tilt) {
        int count = int(worldW / span) + 6;
        for (int i = -count / 2; i <= count / 2; i++) {
            float rx = (i * span);
            float rW = wMin + (wMax - wMin) * Hash01(i * 31 + int(z * 10));
            float rH = hMinH + (hMaxH - hMinH) * Hash01(i * 97 + int(z * 20));
            DrawM(mdlSolid_,
                {rx, yBase + rH * 0.5f, z},
                {rW, rH * 0.5f, 0.22f},
                {0,0,((i & 1) ? tilt : -tilt)});
            // 屋上の小突起（別モデルを小さく）
            DrawM(mdlSwitchBlockOff_,
                {rx + rW * 0.15f, yBase + rH + 0.10f, z - 0.05f},
                {rW * 0.12f, rW * 0.12f, 0.18f},
                {0,0,(i & 1) ? -6.0f : 6.0f});
        }
        };
    DrawBuildings(33.0f, worldH * 0.06f, worldW * 0.14f, worldW * 0.06f, worldW * 0.10f, worldH * 0.14f, worldH * 0.28f, 2.0f);
    DrawBuildings(30.0f, worldH * 0.08f, worldW * 0.12f, worldW * 0.07f, worldW * 0.12f, worldH * 0.18f, worldH * 0.34f, 3.0f);
    DrawBuildings(27.0f, worldH * 0.10f, worldW * 0.10f, worldW * 0.08f, worldW * 0.14f, worldH * 0.22f, worldH * 0.40f, 4.0f);

    // 3) クレーン（梁・支柱）— JumpOnly / Solid を伸ばして構成
    {
        // 支柱
        DrawM(mdlJumpOnly_, {-worldW * 0.30f, worldH * 0.86f, 24.8f}, {0.06f, worldH * 0.55f, 0.30f}, {0,0,0});
        // アーム
        DrawM(mdlSolid_, {-worldW * 0.05f, worldH * 1.05f, 24.6f}, {worldW * 0.55f, 0.06f, 0.30f}, {0,0,-9.0f});
        // ワイヤ
        DrawM(mdlSolid_, {worldW * 0.22f, worldH * 0.88f, 24.5f}, {0.035f, worldH * 0.28f, 0.25f}, {0,0,0});
        // フック
        DrawM(mdlSwitch_, {worldW * 0.22f, worldH * 0.72f, 24.4f}, {0.14f, 0.14f, 0.22f}, {0,0,0});
    }

    // 4) 投光器（本体+三脚・光っぽい板）— Spike を薄く広げてコーン表現
    auto Flood = [&](XMFLOAT3 b, float rotZ) {
        // ポール
        DrawM(mdlSolid_, {b.x, b.y, 22.0f}, {0.05f, 0.55f, 0.25f}, {0,0,0});
        // 本体
        DrawM(mdlSwitchBlockOn_, {b.x, b.y + 0.38f, 21.9f}, {0.22f, 0.12f, 0.22f}, {0,0,rotZ});
        // 光コーン（薄板×2 を少しずらして重ねる）
        DrawM(mdlSpike_, {b.x + 0.10f, b.y + 0.25f, 21.7f}, {worldW * 0.22f, worldH * 0.10f, 0.05f}, {0,0,rotZ - 12.0f});
        DrawM(mdlSpike_, {b.x + 0.08f, b.y + 0.27f, 21.6f}, {worldW * 0.24f, worldH * 0.11f, 0.05f}, {0,0,rotZ - 14.0f});
        };
    Flood({-worldW * 0.48f, worldH * 0.82f, 0}, 10.0f);
    Flood({worldW * 0.52f, worldH * 0.74f, 0}, 18.0f);
    // ========= “3D背景”ここまで =========


    // ===== タイル描画（そのまま） =====
    auto Hash4 = [](int x, int y) {
        uint32_t h = (uint32_t)(x * 73856093u) ^ (uint32_t)(y * 19349663u);
        h ^= (h >> 13); h *= 0x5bd1e995u;
        float r0 = (float)((h) & 0xFF) / 255.0f;
        float r1 = (float)((h >> 8) & 0xFF) / 255.0f;
        float r2 = (float)((h >> 16) & 0xFF) / 255.0f;
        float r3 = (float)((h >> 24) & 0xFF) / 255.0f;
        return XMFLOAT4(r0, r1, r2, r3);
        };

    for (int ty = 0; ty < kMapH; ++ty) {
        for (int tx = 0; tx < kMapW; ++tx) {
            Tile t = grid_[ty][tx];
            if (IsFragile(t) && frag_[ty][tx].gone && t != Tile::Regen) continue;
            if (t == Tile::SwitchBlockOn && !switchOn_) continue;
            if (t == Tile::SwitchBlockOff && switchOn_) continue;

            Model *m = nullptr; bool isFrag = false;
            switch (t) {
            case Tile::Solid:            m = &mdlSolid_; break;
            case Tile::FragileAny:       m = &mdlFragileAny_;    isFrag = true; break;
            case Tile::FragileTop:       m = &mdlFragileTop_;    isFrag = true; break;
            case Tile::FragileBottom:    m = &mdlFragileBottom_; isFrag = true; break;
            case Tile::Regen:            m = &mdlRegen_;         isFrag = true; break;
            case Tile::Spring:           m = &mdlSpring_; break;
            case Tile::Spike:            m = &mdlSpike_; break;
            case Tile::Switch:           m = &mdlSwitch_; break;
            case Tile::SwitchBlockOn:    m = &mdlSwitchBlockOn_; break;
            case Tile::SwitchBlockOff:   m = &mdlSwitchBlockOff_; break;
            case Tile::JumpOnly:         m = &mdlJumpOnly_; break;
            default: break;
            }
            if (!m) continue;

            float wx = xOffset_ + tx * kTile;
            float wy = TyToWorldY(ty);

            Transform base{}; base.pos = {wx + 0.5f * kTile, wy + 0.5f * kTile, 0.0f};
            base.scale = {0.5f, 0.5f, 0.5f * kBlockDepth}; base.rot = {0,0,0};
            {
                XMFLOAT4 r = Hash4(tx, ty);
                base.rot.z = Deg((r.x * 2.0f - 1.0f) * 4.0f);
                base.scale.x *= (1.0f + (r.y * 0.1f - 0.05f));
                base.scale.y *= (1.0f + (r.z * 0.1f - 0.05f));
            }
            renderer->Draw(cmd, *m, base);

            // 以降のガーニッシュはあなたの元コードそのまま（省略していません）
            bool plat = (t == Tile::Solid || t == Tile::JumpOnly || t == Tile::FragileAny || t == Tile::FragileTop || t == Tile::FragileBottom || t == Tile::Regen);
            if (plat) {
                struct C { float ox, oy; }; C cs[4] = {{-0.35f,-0.35f},{0.35f,-0.35f},{0.35f,0.35f},{-0.35f,0.35f}};
                for (auto &c : cs) {
                    Transform leg{}; leg.pos = {base.pos.x + c.ox * base.scale.x, base.pos.y + c.oy * base.scale.y - 0.175f, base.pos.z + 0.05f};
                    leg.scale = {0.03f, 0.175f, base.scale.z * 0.8f}; renderer->Draw(cmd, mdlSolid_, leg);
                }
                Transform tape{}; tape.pos = {base.pos.x, base.pos.y + base.scale.y * 0.8f, base.pos.z - 0.05f};
                tape.scale = {base.scale.x * 0.8f, base.scale.y * 0.18f, base.scale.z}; tape.rot = base.rot; renderer->Draw(cmd, mdlSolid_, tape);
            }

            if (isFrag) {
                auto Crack = [&](float ox, float oy, float hw, float hh, float deg) { Transform c = base; c.pos.x += ox; c.pos.y += oy; c.pos.z -= 0.06f; c.scale.x = hw; c.scale.y = hh; c.scale.z = base.scale.z * 0.6f; c.rot.z = Deg(deg); renderer->Draw(cmd, mdlSolid_, c); };
                XMFLOAT4 r = Hash4(tx * 13 + 7, ty * 17 + 3);
                Crack((r.x * 0.2f - 0.1f), (r.y * 0.2f - 0.1f), 0.28f, 0.03f, r.z * 60.0f - 30.0f);
                Crack((r.y * 0.3f - 0.15f), (r.z * 0.3f - 0.15f), 0.18f, 0.02f, r.w * 100.0f - 50.0f);

                Transform sign{}; sign.pos = {base.pos.x, base.pos.y + base.scale.y * 0.6f, base.pos.z - 0.12f};
                sign.scale = {base.scale.x * 0.45f, base.scale.y * 0.32f, base.scale.z}; sign.rot = {0,0,Deg(12)}; renderer->Draw(cmd, mdlSolid_, sign);
                Transform stick{}; stick.pos = {sign.pos.x, sign.pos.y - sign.scale.y * 0.9f, sign.pos.z + 0.01f}; stick.scale = {sign.scale.x * 0.12f, sign.scale.y * 0.9f, sign.scale.z}; renderer->Draw(cmd, mdlSolid_, stick);
            }

            if (t == Tile::Spike) {
                Transform bar{}; bar.pos = {base.pos.x, base.pos.y + base.scale.y * 0.4f, base.pos.z - 0.07f};
                bar.scale = {base.scale.x * 0.9f, base.scale.y * 0.22f, base.scale.z}; bar.rot = {0,0,Deg(-6)}; renderer->Draw(cmd, mdlSolid_, bar);
                auto Leg = [&](float s) { Transform l{}; l.pos = {base.pos.x + base.scale.x * 0.7f * s, base.pos.y - base.scale.y * 0.1f, base.pos.z - 0.05f}; l.scale = {base.scale.x * 0.18f, base.scale.y * 0.7f, base.scale.z}; l.rot = {0,0,Deg(15 * s)}; renderer->Draw(cmd, mdlSolid_, l); };
                Leg(-1.0f); Leg(+1.0f);
            }

            if (t == Tile::Spring) {
                Transform basePlate{}; basePlate.pos = {base.pos.x, base.pos.y - base.scale.y * 0.6f, base.pos.z - 0.05f}; basePlate.scale = {base.scale.x * 0.8f, base.scale.y * 0.4f, base.scale.z}; renderer->Draw(cmd, mdlSolid_, basePlate);
                Transform pillar{}; pillar.pos = {base.pos.x, base.pos.y + base.scale.y * 0.1f, base.pos.z - 0.07f}; pillar.scale = {base.scale.x * 0.25f, base.scale.y * 0.9f, base.scale.z}; renderer->Draw(cmd, mdlSolid_, pillar);
                Transform head{}; head.pos = {base.pos.x, base.pos.y + base.scale.y * 0.9f, base.pos.z - 0.09f}; head.scale = {base.scale.x * 0.7f, base.scale.y * 0.25f, base.scale.z}; head.rot = {0,0,Deg(5)}; renderer->Draw(cmd, mdlSolid_, head);
            }

            if (t == Tile::Switch) {
                Transform box{}; box.pos = {base.pos.x + base.scale.x * 0.7f, base.pos.y + base.scale.y * 0.2f, base.pos.z - 0.06f}; box.scale = {base.scale.x * 0.45f, base.scale.y * 0.55f, base.scale.z}; box.rot = {0,0,Deg(-5)}; renderer->Draw(cmd, mdlSolid_, box);
                Transform lever{}; lever.pos = {box.pos.x + box.scale.x * 0.3f, box.pos.y + box.scale.y * 0.2f, box.pos.z - 0.02f}; lever.scale = {box.scale.x * 0.25f, box.scale.y * 0.6f, box.scale.z}; lever.rot = {0,0,Deg(switchOn_ ? 30.0f : -30.0f)}; renderer->Draw(cmd, mdlSolid_, lever);
            }
        }
    }

    // ===== プレイヤー =====
    {
        float s = 0.5f; XMFLOAT3 mn = playerModel_.GetLocalMin();
        Transform p{}; p.pos = {playerTr_.pos.x + pw_ * 0.5f, playerTr_.pos.y - (mn.y * s), kPlayerZ}; p.scale = {s,s,s * kPlayerDepth}; p.rot = {0,0,0};
        renderer->Draw(cmd, playerModel_, p);

        Transform belt{}; belt.pos = {p.pos.x, p.pos.y + p.scale.y * 0.2f, p.pos.z - 0.03f}; belt.scale = {p.scale.x * 0.7f, p.scale.y * 0.12f, p.scale.z}; belt.rot = {0,0,Deg(8)}; renderer->Draw(cmd, mdlSolid_, belt);
    }

    // ===== 手前ガーニッシュ（ちょい前景・プレイヤーより手前に） =====
    {
        float y = -0.5f * kTile;
        Transform rail{}; rail.pos = {0.0f, y, -0.40f}; rail.scale = {worldW * 0.65f, 0.05f, 0.22f};
        renderer->Draw(cmd, mdlSolid_, rail);
        for (int i = -3; i <= 3; ++i) {
            float x = i * (worldW * 0.16f);
            Transform t{}; t.pos = {x, y + 0.2f, -0.41f}; t.scale = {worldW * 0.09f, 0.03f, 0.22f}; t.rot = {0,0,Deg((i % 2 == 0) ? -10.0f : 12.0f)};
            renderer->Draw(cmd, mdlSolid_, t);
        }
    }

    renderer->End(cmd);

    // HUD / Editor
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
    // ComPtrで自動解放
}
