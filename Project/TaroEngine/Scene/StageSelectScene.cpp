#define NOMINMAX
#include "StageSelectScene.h"

#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cmath>

#include "DirectXCommon.h"
#include "ModelRenderer.h"
#include "SceneManager.h"
#include "Input.h"
#include "GameScene.h"
#include "TitleScene.h"

#include <DirectXTex/DirectXTex.h>
#include <DirectXTex/d3dx12.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;


// ===== UTF-8→UTF-16 =====
std::wstring StageSelectScene::Widen_(const std::string &s) {
    if (s.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// ===== テクスチャSRV作成 =====
bool StageSelectScene::LoadTextureSRV_(
    const std::wstring &fileU16,
    UINT srvIndex,
    ComPtr<ID3D12Resource> &outTex,
    D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle) {

    auto *dx = engine_->directXCommon;
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

    const auto &meta = img.GetMetadata();
    DXGI_FORMAT fmt = meta.format;

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

    const DirectX::Image *src = conv.GetImages() ? conv.GetImages() : img.GetImages();
    auto useMeta = conv.GetMetadata().width ? conv.GetMetadata() : meta;
    fmt = useMeta.format;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = (UINT)useMeta.width;
    desc.Height = (UINT)useMeta.height;
    desc.DepthOrArraySize = (UINT16)useMeta.arraySize;
    desc.MipLevels = (UINT16)(useMeta.mipLevels ? useMeta.mipLevels : 1);
    desc.Format = fmt;
    desc.SampleDesc = {1,0};
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    hr = device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&outTex)
    );
    if (FAILED(hr)) return false;

    // アップロード
    UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);
    ComPtr<ID3D12Resource> upload = BufferUtility::CreateUploadBuffer(device, uploadSize);

    // ワンショットcmd
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
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        alloc.Get(), nullptr,
        IID_PPV_ARGS(&list)
    );

    std::vector<D3D12_SUBRESOURCE_DATA> subs((size_t)useMeta.mipLevels);
    for (size_t m = 0; m < useMeta.mipLevels; ++m) {
        const DirectX::Image &im = src[m];
        subs[m].pData = im.pixels;
        subs[m].RowPitch = im.rowPitch;
        subs[m].SlicePitch = im.slicePitch;
    }

    UpdateSubresources(
        list.Get(), outTex.Get(), upload.Get(),
        0, 0,
        (UINT)useMeta.mipLevels,
        subs.data()
    );

    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        outTex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    list->ResourceBarrier(1, &toSRV);
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

    // SRV作成
    ID3D12DescriptorHeap *heapSrv = dx->GetSrvHeap();
    UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = heapSrv->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += SIZE_T(inc) * srvIndex;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = heapSrv->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(inc) * srvIndex;

    outGpuHandle = gpu;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = fmt;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = desc.MipLevels;
    device->CreateShaderResourceView(outTex.Get(), &srv, cpu);

    return true;
}

// ===== カメラ更新 =====
void StageSelectScene::RefreshCameraOrtho_() {
    auto *dx = engine_->directXCommon;
    float w = (float)dx->GetWidth();
    float h = (float)dx->GetHeight();
    if (w <= 0 || h <= 0) return;

    float aspect = w / std::max(1.0f, h);
    if (camera_.IsOrtho()) {
        camera_.SetOrthoViewSize(virtualWorldH_ * aspect, virtualWorldH_);
    }
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
}

// ===== モデル描画ヘルパ =====
void StageSelectScene::DrawModel_(
    Model &m,
    const XMFLOAT3 &pos,
    const XMFLOAT3 &fullScale,
    const XMFLOAT3 &rotDeg) {

    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    auto Deg = [](float d) { return XMConvertToRadians(d); };

    Transform t{};
    t.pos = pos;
    t.scale = {
        fullScale.x * 0.5f,
        fullScale.y * 0.5f,
        fullScale.z * 0.5f
    };
    t.rot = {
        Deg(rotDeg.x),
        Deg(rotDeg.y),
        Deg(rotDeg.z)
    };

    mr->Draw(cmd, m, t);
}

// ===== Initialize =====
void StageSelectScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engine_ = engine;
    render_ = render;

    auto *dx = engine_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // モデルロード
    mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");
    mdlFragileAny_.Initialize(device, "Resources/Model/Block/fragile_any.obj");
    mdlFragileTop_.Initialize(device, "Resources/Model/Block/fragile_top.obj");
    mdlFragileBottom_.Initialize(device, "Resources/Model/Block/fragile_bottom.obj");
    mdlRegen_.Initialize(device, "Resources/Model/Block/regen.obj");
    mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
    mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");
    mdlSwitchOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");
    mdlSwitchBlockOn_.Initialize(device, "Resources/Model/Block/switchblock_on.obj");
    mdlSwitchBlockOff_.Initialize(device, "Resources/Model/Block/switchblock_off.obj");
    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");

    // ★キー文字モデルは中身が空で落ちるので、いったんロードしない
    // mdlKeyA_.Initialize(device, "Resources/Model/a.obj");
    // mdlKeyD_.Initialize(device, "Resources/Model/d.obj");
    // mdlKeyW_.Initialize(device, "Resources/Model/w.obj");
    // mdlKeyS_.Initialize(device, "Resources/Model/s.obj");

    // テクスチャSRV割り当て
    auto setupTex = [&](Model &m, UINT slot, ComPtr<ID3D12Resource> &holder) {
        if (m.GetAlbedoPath().empty()) return;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        if (LoadTextureSRV_(Widen_(m.GetAlbedoPath()), slot, holder, gpu)) {
            m.SetAlbedoSRV(gpu);
        }
        };
    setupTex(mdlSolid_, kSrv_T_Solid, texSolid_);
    setupTex(mdlFragileAny_, kSrv_T_FragileAny, texFragileAny_);
    setupTex(mdlFragileTop_, kSrv_T_FragileTop, texFragileTop_);
    setupTex(mdlFragileBottom_, kSrv_T_FragileBottom, texFragileBottom_);
    setupTex(mdlRegen_, kSrv_T_Regen, texRegen_);
    setupTex(mdlSpring_, kSrv_T_Spring, texSpring_);
    setupTex(mdlSpike_, kSrv_T_Spike, texSpike_);
    setupTex(mdlSwitchOn_, kSrv_T_SwitchOn, texSwitchOn_);
    setupTex(mdlSwitchOff_, kSrv_T_SwitchOff, texSwitchOff_);
    setupTex(mdlSwitchBlockOn_, kSrv_T_SwitchBlockOn, texSwitchBlockOn_);
    setupTex(mdlSwitchBlockOff_, kSrv_T_SwitchBlockOff, texSwitchBlockOff_);
    setupTex(mdlJumpOnly_, kSrv_T_JumpOnly, texJumpOnly_);

    // カメラ設定（正射影）
    float w = (float)dx->GetWidth();
    float h = (float)dx->GetHeight();
    float aspect = std::max(1.0f, w) / std::max(1.0f, h);

    float orthoW = virtualWorldH_ * aspect;
    float orthoH = virtualWorldH_;

    camera_.Initialize(
        {0.0f, orthoH * 0.5f, -50.0f},
        {0,0,0},
        60.0f, aspect, 0.1f, 1000.0f
    );
    camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);
    camera_.LookAt(
        {0.0f, orthoH * 0.5f, -50.0f},
        {0.0f, orthoH * 0.5f,   0.0f}
    );
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    blinkTime_ = 0.0f;
    blinkStrength_ = 0.0f;

    // ステージ/難易度 初期値
    curStage_ = std::clamp(startStage_, kMinStage_, kMaxStage_);
    curDiff_ = startDiff_;

    // プレビューの横オフセット（マップ中心揃え）
    const float mapW = kMapW * kTile;
    xOffsetPreview_ = -mapW * 0.5f;

    // CSV読み込み
    LoadPreviewFromCSV_();
}

// ===== CSVを読んで previewGrid_ にタイルを入れる =====
void StageSelectScene::LoadPreviewFromCSV_() {
    // 全部Emptyで初期化
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            previewGrid_[y][x] = Tile::Empty;
        }
    }

    // 難易度タグ
    auto diffTag = [this]() -> const char * {
        switch (curDiff_) {
        case Difficulty::Easy:   return "easy";
        case Difficulty::Normal: return "normal";
        case Difficulty::Hard:   return "hard";
        }
        return "normal";
        }();

    // 優先: stageNN_<diff>.csv
    char pathBuf[64];
    std::snprintf(pathBuf, sizeof(pathBuf),
        "stage%02d_%s.csv", curStage_, diffTag);

    std::ifstream ifs(pathBuf);

    // フォールバック: stageNN.csv
    if (!ifs) {
        std::snprintf(pathBuf, sizeof(pathBuf),
            "stage%02d.csv", curStage_);
        ifs.open(pathBuf);
    }

    if (!ifs) {
        // ファイルが無いなら空プレビューのまま
        return;
    }

    std::string line;
    // 1行目 (W,H,spawnTx,spawnTy...) は読み飛ばす
    if (!std::getline(ifs, line)) return;

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
            previewGrid_[y][x] = (Tile)id;
            ++x;
        }
        ++y;
    }
}

// ===== Update =====
void StageSelectScene::Update(float dt) {
    RefreshCameraOrtho_();

    blinkTime_ += dt;
    float basePulse = 0.5f * (1.0f + std::sinf(blinkTime_ * 3.0f));
    blinkStrength_ = std::pow(basePulse, 3.0f); // 点滅用

    auto *in = engine_->input;

    // --- ステージ番号変更 (A / D) ---
    if (in->IsKeyTriggered(DIK_A)) {
        curStage_--;
        if (curStage_ < kMinStage_) curStage_ = kMinStage_;
        LoadPreviewFromCSV_();
    }
    if (in->IsKeyTriggered(DIK_D)) {
        curStage_++;
        if (curStage_ > kMaxStage_) curStage_ = kMaxStage_;
        LoadPreviewFromCSV_();
    }

    // --- 難易度変更 (W / S) ---
    if (in->IsKeyTriggered(DIK_W)) {
        int d = (int)curDiff_;
        d--;
        if (d < 0) d = 0;
        curDiff_ = (Difficulty)d;
        LoadPreviewFromCSV_();
    }
    if (in->IsKeyTriggered(DIK_S)) {
        int d = (int)curDiff_;
        d++;
        if (d > 2) d = 2;
        curDiff_ = (Difficulty)d;
        LoadPreviewFromCSV_();
    }

    // --- 決定 (SPACE) ---
    if (in->IsKeyTriggered(DIK_SPACE)) {
        engine_->sceneManager->ChangeScene(
            std::make_unique<GameScene>(curStage_, curDiff_)
        );
        return;
    }
}

// ===== 背景（夜の工事現場っぽいやつ） =====
void StageSelectScene::DrawBackgroundLayers_(float W, float H) {
    auto Lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // 空レイヤ
    DrawModel_(mdlSpring_,
        {0.0f, H * 0.50f, 38.0f},
        {W * 2.6f, H * 2.2f, 0.25f},
        {0,0,0});

    DrawModel_(mdlJumpOnly_,
        {0.0f, H * 0.48f, 37.8f},
        {W * 2.6f, H * 1.9f, 0.25f},
        {0,0,-4.0f});

    for (int i = -3; i <= 3; ++i) {
        float rx = i * (W * 0.35f);
        float ry = H * (0.70f + 0.03f * std::sin(i * 0.9f));
        float rw = W * Lerp(0.50f, 0.80f, ((i * 31 * 2654435761u) & 255) / 255.0f);
        float rh = H * 0.06f;
        float rz = 37.6f - (i & 1) * 0.2f;
        DrawModel_(mdlSpring_,
            {rx, ry, rz},
            {rw, rh, 0.05f},
            {0, 0, (i & 1) ? -8.0f : 10.0f});
    }

    // ビル群
    auto DrawBuildings = [&](float z, float yBase, float span,
        float wMin, float wMax,
        float hMin, float hMax,
        float tiltDeg, bool warnLight) {
            int count = int(W / span) + 8;
            for (int i = -count / 2; i <= count / 2; ++i) {
                float rx = i * span;
                uint32_t seed = (uint32_t)(i * 73 + (int)(z * 10));
                float r = (float)(seed & 0xFFFF) / 65535.0f;

                float rw = Lerp(wMin, wMax, r);
                float rh = Lerp(hMin, hMax, 1.0f - r);

                DrawModel_(mdlSolid_,
                    {rx, yBase + rh * 0.5f, z},
                    {rw, rh * 0.5f, 0.22f},
                    {0,0,(i & 1) ? tiltDeg : -tiltDeg});

                // 屋上の小物（OFFなスイッチ床）
                DrawModel_(mdlSwitchBlockOff_,
                    {rx + rw * 0.14f, yBase + rh + 0.12f, z - 0.03f},
                    {rw * 0.12f, rw * 0.12f, 0.18f},
                    {0,0,(i & 1) ? -6.0f : 8.0f});

                // 警告灯っぽいやつ（点灯ブロック等）
                if (warnLight && ((i + (int)z) % 5 == 0)) {
                    DrawModel_(mdlSwitchBlockOn_,
                        {rx, yBase + rh + 0.24f, z - 0.05f},
                        {0.10f, 0.10f, 0.15f},
                        {0,0,0});
                }
            }
        };

    DrawBuildings(
        33.0f, H * 0.06f,
        W * 0.14f,
        W * 0.06f, W * 0.10f,
        H * 0.16f, H * 0.30f,
        2.0f, true
    );
    DrawBuildings(
        29.0f, H * 0.08f,
        W * 0.12f,
        W * 0.07f, W * 0.12f,
        H * 0.18f, H * 0.36f,
        3.0f, true
    );
    DrawBuildings(
        25.0f, H * 0.10f,
        W * 0.10f,
        W * 0.08f, W * 0.14f,
        H * 0.22f, H * 0.40f,
        4.0f, false
    );

    // クレーン周り（制御盤とかライト）
    DrawModel_(mdlJumpOnly_,
        {-W * 0.32f, H * 0.86f, 24.8f},
        {0.06f, H * 0.55f, 0.30f},
        {0,0,0});

    DrawModel_(mdlSolid_,
        {-W * 0.06f, H * 1.03f, 24.6f},
        {W * 0.55f, 0.06f, 0.30f},
        {0,0,-9.0f});

    DrawModel_(mdlSolid_,
        {W * 0.22f, H * 0.88f, 24.5f},
        {0.035f,  H * 0.28f, 0.25f},
        {0,0,0});

    // 制御盤
    DrawModel_(mdlSwitchOff_,
        {W * 0.22f, H * 0.72f, 24.4f},
        {0.14f,   0.14f,    0.22f},
        {0,0,0});

    // 吊り荷
    DrawModel_(mdlSolid_,
        {W * 0.22f, H * 0.55f, 24.3f},
        {0.35f, 0.08f, 0.25f},
        {0,0, 4.0f});

    // 投光器
    DrawModel_(mdlSwitchOn_,
        {W * 0.22f, H * 0.47f, 24.2f},
        {0.15f, 0.08f, 0.22f},
        {0,0, 0.0f});

    // 前景の手すり
    {
        float railY = -0.6f;

        DrawModel_(mdlSolid_,
            {0.0f, railY, -0.40f},
            {W * 0.66f, 0.05f, 0.22f},
            {0,0,0});

        for (int i = -3; i <= 3; ++i) {
            float x = i * (W * 0.16f);
            DrawModel_(mdlSolid_,
                {x, railY + 0.20f, -0.41f},
                {W * 0.09f, 0.03f, 0.22f},
                {0, 0, (i % 2 == 0) ? -10.0f : 12.0f});
        }

        for (int i = -2; i <= 2; ++i) {
            DrawModel_(mdlJumpOnly_,
                {i * (W * 0.18f), railY + 0.55f, -0.42f},
                {W * 0.08f, 0.01f, 0.2f},
                {0,0, 10.0f * std::sinf(float(i))});
        }
    }

    // 両サイドの投光器・パネル
    auto Flood = [&](XMFLOAT3 b, float rotZ) {
        // ポール
        DrawModel_(mdlSolid_,
            {b.x, b.y, 22.0f},
            {0.05f, 0.55f, 0.25f},
            {0,0,0});

        // ヘッド
        DrawModel_(mdlSwitchOn_,
            {b.x, b.y + 0.38f, 21.9f},
            {0.22f, 0.12f, 0.22f},
            {0,0, rotZ});

        // 下の警告板
        DrawModel_(mdlSpike_,
            {b.x + 0.10f, b.y + 0.26f, 21.7f},
            {W * 0.22f, H * 0.10f, 0.05f},
            {0,0, rotZ - 12.0f});
        DrawModel_(mdlSpike_,
            {b.x + 0.08f, b.y + 0.28f, 21.6f},
            {W * 0.24f, H * 0.11f, 0.05f},
            {0,0, rotZ - 14.0f});
        };
    Flood({-W * 0.48f, H * 0.82f, 0}, 10.0f);
    Flood({W * 0.52f, H * 0.74f, 0}, 18.0f);
}

// ===== "STAGE XX" のバナー =====
void StageSelectScene::DrawStageNumberBanner_(float W, float H) {
    float originY = H * (-0.10f); // 画面下寄せ
    float zBase = -0.6f;
    float glyphW = W * 0.03f;
    float glyphH = H * 0.03f;
    float span = glyphW * 1.2f;

    // 点滅
    float glowScaleMul = 1.0f + blinkStrength_ * 0.12f;
    float glowZAdd = 0.02f + blinkStrength_ * 0.03f;
    float subtleFloat = (blinkStrength_ - 0.5f) * 0.06f;

    auto DrawBarBase = [&](float cx, float cy, float w, float h, float rotDegZ = 0.0f) {
        DrawModel_(mdlFragileAny_,
            {cx, cy, zBase},
            {w, h, 0.05f},
            {0,0,rotDegZ});
        };
    auto DrawBarGlow = [&](float cx, float cy, float w, float h, float rotDegZ = 0.0f) {
        DrawModel_(mdlSwitchOn_,
            {cx, cy + subtleFloat, zBase + glowZAdd},
            {w * glowScaleMul, h * 0.4f, 0.05f},
            {0,0,rotDegZ});
        };

    auto Draw_S = [&](float gx) {
        DrawBarBase(gx, originY + glyphH * 0.30f, glyphW * 0.8f, glyphH * 0.18f);
        DrawBarBase(gx + glyphW * 0.15f, originY + 0.0f, glyphW * 0.7f, glyphH * 0.18f);
        DrawBarBase(gx, originY - glyphH * 0.30f, glyphW * 0.8f, glyphH * 0.18f);
        DrawBarBase(gx - glyphW * 0.30f, originY + glyphH * 0.15f, glyphW * 0.18f, glyphH * 0.30f);
        DrawBarBase(gx + glyphW * 0.30f, originY - glyphH * 0.15f, glyphW * 0.18f, glyphH * 0.30f);
        DrawBarGlow(gx, originY + glyphH * 0.30f, glyphW * 0.82f, glyphH * 0.07f);
        };
    auto Draw_T = [&](float gx) {
        DrawBarBase(gx, originY + glyphH * 0.30f, glyphW * 0.9f, glyphH * 0.18f);
        DrawBarBase(gx, originY - glyphH * 0.05f, glyphW * 0.20f, glyphH * 0.80f);
        DrawBarGlow(gx, originY + glyphH * 0.30f, glyphW * 0.92f, glyphH * 0.07f);
        };
    auto Draw_A = [&](float gx) {
        DrawBarBase(gx - glyphW * 0.25f, originY - glyphH * 0.05f, glyphW * 0.20f, glyphH * 0.80f, -8.0f);
        DrawBarBase(gx + glyphW * 0.25f, originY - glyphH * 0.05f, glyphW * 0.20f, glyphH * 0.80f, 8.0f);
        DrawBarBase(gx, originY + glyphH * 0.05f, glyphW * 0.60f, glyphH * 0.18f);
        DrawBarGlow(gx, originY + glyphH * 0.05f, glyphW * 0.62f, glyphH * 0.06f);
        };
    auto Draw_G = [&](float gx) {
        DrawBarBase(gx - glyphW * 0.30f, originY, glyphW * 0.20f, glyphH * 0.80f);
        DrawBarBase(gx, originY + glyphH * 0.30f, glyphW * 0.70f, glyphH * 0.20f);
        DrawBarBase(gx, originY - glyphH * 0.30f, glyphW * 0.70f, glyphH * 0.20f);
        DrawBarBase(gx + glyphW * 0.15f, originY - glyphH * 0.05f, glyphW * 0.45f, glyphH * 0.18f);
        DrawBarGlow(gx, originY + glyphH * 0.30f, glyphW * 0.72f, glyphH * 0.07f);
        };
    auto Draw_E = [&](float gx) {
        DrawBarBase(gx - glyphW * 0.30f, originY, glyphW * 0.20f, glyphH * 0.80f);
        DrawBarBase(gx, originY + glyphH * 0.30f, glyphW * 0.70f, glyphH * 0.20f);
        DrawBarBase(gx, originY + 0.00f, glyphW * 0.60f, glyphH * 0.18f);
        DrawBarBase(gx, originY - glyphH * 0.30f, glyphW * 0.70f, glyphH * 0.20f);
        DrawBarGlow(gx, originY + 0.00f, glyphW * 0.62f, glyphH * 0.06f);
        };

    auto DrawDigit = [&](float gx, int digit) {
        bool segA = false, segB = false, segC = false, segD = false, segE = false, segF = false, segG = false;
        switch (digit) {
        case 0: segA = segB = segC = segD = segE = segF = true; break;
        case 1: segB = segC = true; break;
        case 2: segA = segB = segG = segE = segD = true; break;
        case 3: segA = segB = segC = segD = segG = true; break;
        case 4: segF = segG = segB = segC = true; break;
        case 5: segA = segF = segG = segC = segD = true; break;
        case 6: segA = segF = segE = segD = segC = segG = true; break;
        case 7: segA = segB = segC = true; break;
        case 8: segA = segB = segC = segD = segE = segF = segG = true; break;
        case 9: segA = segB = segC = segD = segF = segG = true; break;
        default: break;
        }

        auto Bar = [&](float ox, float oy, float w, float h, float rz, bool on) {
            if (!on) return;
            DrawModel_(mdlFragileAny_,
                {gx + ox, originY + oy, -0.6f},
                {glyphW * w, glyphH * h, 0.05f},
                {0,0,rz});
            DrawModel_(mdlSwitchOn_,
                {gx + ox,
                 originY + oy + (blinkStrength_ - 0.5f) * 0.06f,
                 -0.6f + 0.02f + blinkStrength_ * 0.03f},
                {glyphW * w * (1.0f + blinkStrength_ * 0.12f),
                 glyphH * h * 0.4f,
                 0.05f},
                {0,0,rz});
            };

        // 上
        Bar(0.0f, +glyphH * 0.30f, 0.8f, 0.18f, 0.0f, segA);
        // 中
        Bar(0.0f, 0.0f, 0.8f, 0.18f, 0.0f, segG);
        // 下
        Bar(0.0f, -glyphH * 0.30f, 0.8f, 0.18f, 0.0f, segD);
        // 左上
        Bar(-glyphW * 0.30f, +glyphH * 0.15f, 0.18f, 0.30f, 0.0f, segF);
        // 左下
        Bar(-glyphW * 0.30f, -glyphH * 0.15f, 0.18f, 0.30f, 0.0f, segE);
        // 右上
        Bar(+glyphW * 0.30f, +glyphH * 0.15f, 0.18f, 0.30f, 0.0f, segB);
        // 右下
        Bar(+glyphW * 0.30f, -glyphH * 0.15f, 0.18f, 0.30f, 0.0f, segC);
        };

    float startX = -span * 3.5f;
    Draw_S(startX + span * 0.0f);
    Draw_T(startX + span * 1.0f);
    Draw_A(startX + span * 2.0f);
    Draw_G(startX + span * 3.0f);
    Draw_E(startX + span * 4.0f);

    int tens = (curStage_ / 10) % 10;
    int ones = (curStage_ % 10);
    float numStartX = startX + span * 6.5f;
    if (curStage_ >= 10) {
        DrawDigit(numStartX, tens);
        DrawDigit(numStartX + span * 1.0f, ones);
    } else {
        DrawDigit(numStartX, ones);
    }
}

// ===== ステージのミニチュアプレビュー描画 =====
void StageSelectScene::DrawPreviewMiniMap_(float /*W*/, float H) {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    auto Deg = [](float d) { return DirectX::XMConvertToRadians(d); };

    const float scaleTile = 0.75f;
    const float baseZ = 5.0f;
    const float baseY = H * 0.40f;
    const float baseX = 0.0f;

    auto DrawOneTile = [&](Model &m,
        const DirectX::XMFLOAT3 &pos,
        float zrotDeg,
        bool crackOrDeco) {

            Transform t{};
            t.pos = pos;
            t.scale = {
                0.5f * scaleTile,
                0.5f * scaleTile,
                0.5f * scaleTile
            };
            t.rot = {0,0,Deg(zrotDeg)};
            mr->Draw(cmd, m, t);

            if (crackOrDeco) {
                Transform sign{};
                sign.pos = {
                    pos.x,
                    pos.y + 0.30f * scaleTile,
                    pos.z - 0.05f
                };
                sign.scale = {
                    0.20f * scaleTile,
                    0.15f * scaleTile,
                    0.20f * scaleTile
                };
                sign.rot = {0,0,Deg(12)};
                mr->Draw(cmd, mdlSolid_, sign);
            }
        };

    for (int ty = 0; ty < kMapH; ++ty) {
        for (int tx = 0; tx < kMapW; ++tx) {
            Tile t = previewGrid_[ty][tx];
            Model *m = nullptr;
            bool showCrack = false;

            switch (t) {
            case Tile::Solid:
                m = &mdlSolid_;
                break;
            case Tile::FragileAny:
                m = &mdlFragileAny_;
                showCrack = true;
                break;
            case Tile::FragileTop:
                m = &mdlFragileTop_;
                showCrack = true;
                break;
            case Tile::FragileBottom:
                m = &mdlFragileBottom_;
                showCrack = true;
                break;
            case Tile::Regen:
                m = &mdlRegen_;
                showCrack = true;
                break;
            case Tile::Spring:
                m = &mdlSpring_;
                break;
            case Tile::Spike:
                m = &mdlSpike_;
                break;
            case Tile::JumpOnly:
                m = &mdlJumpOnly_;
                break;
            case Tile::Switch:
                m = &mdlSwitchOn_;
                break;
            case Tile::SwitchBlockOn:
                m = &mdlSwitchBlockOn_;
                break;
            case Tile::SwitchBlockOff:
                m = &mdlSwitchBlockOff_;
                break;
            default:
                break;
            }

            if (!m) continue;

            float worldX = xOffsetPreview_ + tx * kTile;
            float worldY = (float)(kMapH - 1 - ty) * kTile;

            float drawX = baseX + worldX * scaleTile;
            float drawY = baseY + worldY * scaleTile;
            float drawZ = baseZ;

            float perTileRot = ((tx + ty) & 1) ? 4.0f : -4.0f;

            DrawOneTile(
                *m,
                {drawX, drawY, drawZ},
                perTileRot,
                showCrack
            );
        }
    }
}

// ===== HUD: 操作説明用の板だけ =====
void StageSelectScene::DrawControlHelp3D_(float W, float H) {
    // 画面下あたりに、"A  ←" / "D  →" / "W ↑" / "S ↓" / "START" っぽい板を並べる
    // 文字モデルなし版なので、ひとまずパネルの「点滅 = 有効」だけ表す

    const float hudY = -0.2f * H;
    const float hudZ = -0.9f;

    // ステージ進められるか？
    bool canLeft = (curStage_ > kMinStage_);
    bool canRight = (curStage_ < kMaxStage_);

    // 難易度動かせるか？
    int dNow = (int)curDiff_;
    bool canUp = (dNow > 0);
    bool canDown = (dNow < 2);

    // ▼共通の小パネル描画
    auto DrawPanel = [&](float cx, float cy, float cz,
        float sx, float sy,
        float rotZdeg,
        bool blink) {

            // ベース板
            DrawModel_(mdlSolid_,
                {cx, cy, cz},
                {sx, sy, 0.15f},
                {0,0,rotZdeg});

            // 点滅ハイライト
            if (blink) {
                float floatY = (blinkStrength_ - 0.5f) * 0.06f;
                DrawModel_(mdlSwitchOn_,
                    {cx, cy + floatY, cz + 0.05f},
                    {sx * (1.0f + blinkStrength_ * 0.12f),
                     sy * 0.5f,
                     0.10f},
                    {0,0,rotZdeg});
            }
        };

    // --- 左（Aでステージ--）: 左側に2枚のパネルを斜め置き
    {
        float xBase = -W * 0.30f;
        float yBase = hudY;

        // 大きめ
        DrawPanel(
            xBase,
            yBase,
            hudZ,
            W * 0.10f,
            H * 0.04f,
            -20.0f,
            canLeft
        );

        // 小さめをちょっとズラして重ねる（←←っぽい）
        DrawPanel(
            xBase + W * 0.03f,
            yBase + H * 0.015f,
            hudZ + 0.02f,
            W * 0.06f,
            H * 0.025f,
            -20.0f,
            canLeft
        );
    }

    // --- 右（Dでステージ++）
    {
        float xBase = W * 0.30f;
        float yBase = hudY;

        DrawPanel(
            xBase,
            yBase,
            hudZ,
            W * 0.10f,
            H * 0.04f,
            20.0f,
            canRight
        );

        DrawPanel(
            xBase - W * 0.03f,
            yBase + H * 0.015f,
            hudZ + 0.02f,
            W * 0.06f,
            H * 0.025f,
            20.0f,
            canRight
        );
    }

    // --- 上方向（Wで難易度アップ）
    {
        float xBase = 0.0f;
        float yBase = hudY + H * 0.10f;

        DrawPanel(
            xBase,
            yBase - H * 0.06f,
            hudZ,
            W * 0.09f,
            H * 0.035f,
            -8.0f,
            canUp
        );

        DrawPanel(
            xBase + W * 0.02f,
            yBase - H * 0.03f,
            hudZ + 0.02f,
            W * 0.05f,
            H * 0.02f,
            -8.0f,
            canUp
        );
    }

    // --- 下方向（Sで難易度ダウン）
    {
        float xBase = 0.0f;
        float yBase = hudY - H * 0.10f;

        DrawPanel(
            xBase,
            yBase + H * 0.06f,
            hudZ,
            W * 0.09f,
            H * 0.035f,
            8.0f,
            canDown
        );

        DrawPanel(
            xBase - W * 0.02f,
            yBase + H * 0.03f,
            hudZ + 0.02f,
            W * 0.05f,
            H * 0.02f,
            8.0f,
            canDown
        );
    }

    // --- 真ん中（SPACEでスタート） 常に点滅
    {
        float xC = 0.0f;
        float yC = hudY;
        float zC = hudZ + 0.05f;

        DrawPanel(
            xC,
            yC,
            zC,
            W * 0.18f,
            H * 0.06f,
            0.0f,
            true
        );

        DrawPanel(
            xC,
            yC + H * 0.025f,
            zC + 0.03f,
            W * 0.20f,
            H * 0.015f,
            0.0f,
            true
        );
    }
}

// ===== Draw =====
void StageSelectScene::Draw() {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    mr->Begin(cmd, dx, camera_);

    float aspect =
        std::max(1.0f, (float)dx->GetWidth()) /
        std::max(1.0f, (float)dx->GetHeight());
    float W = virtualWorldH_ * aspect;
    float H = virtualWorldH_;

    // 背景
    DrawBackgroundLayers_(W, H);

    // ステージプレビュー（ミニマップ）
    DrawPreviewMiniMap_(W, H);

    // 下の "STAGE XX"
    DrawStageNumberBanner_(W, H);

    // 操作説明HUD（板ベース）
    DrawControlHelp3D_(W, H);

    mr->End(cmd);
}

// ===== Finalize =====
void StageSelectScene::Finalize() {
    // ComPtrが自動解放されるので特に何もしない
}

// 難易度テキスト
const char *StageSelectScene::DiffToText_(Difficulty d) {
    switch (d) {
    case Difficulty::Easy:   return "EASY";
    case Difficulty::Normal: return "NORMAL";
    case Difficulty::Hard:   return "HARD";
    }
    return "UNKNOWN";
}
