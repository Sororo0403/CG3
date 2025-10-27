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
#include "imgui/imgui.h"

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
bool StageSelectScene::LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
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

    if (!DirectX::IsCompressed(meta.format) &&
        !DirectX::IsSRGB(meta.format)) {
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

    // モデルロード（GameScene/TitleSceneと同じファイル名を想定）
    mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");
    mdlFragileAny_.Initialize(device, "Resources/Model/Block/fragile_any.obj");
    mdlFragileTop_.Initialize(device, "Resources/Model/Block/fragile_top.obj");
    mdlFragileBottom_.Initialize(device, "Resources/Model/Block/fragile_bottom.obj");
    mdlRegen_.Initialize(device, "Resources/Model/Block/regen.obj");
    mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
    mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");
    mdlSwitch_.Initialize(device, "Resources/Model/Block/switch.obj");
    mdlSwitchOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");
    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");

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
    setupTex(mdlSwitch_, kSrv_T_Switch, texSwitch_);
    setupTex(mdlSwitchOn_, kSrv_T_SwitchOn, texSwitchOn_);
    setupTex(mdlSwitchOff_, kSrv_T_SwitchOff, texSwitchOff_);
    setupTex(mdlJumpOnly_, kSrv_T_JumpOnly, texJumpOnly_);

    // カメラはTitleSceneと同じ正射影系
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

    // ステージ1から
    curStage_ = 1;

    // プレビュー用のオフセット（マップを中央に寄せたい）
    const float mapW = kMapW * kTile;
    xOffsetPreview_ = -mapW * 0.5f;

    LoadPreviewFromCSV_();
}

// ===== CSV読み込みしてpreviewGrid_に詰める =====
void StageSelectScene::LoadPreviewFromCSV_() {
    // 初期化
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            previewGrid_[y][x] = Tile::Empty;
        }
    }
    switchOnPreview_ = false;

    // "stageXX.csv"
    char pathBuf[64];
    std::snprintf(pathBuf, sizeof(pathBuf), "stage%02d.csv", curStage_);

    std::ifstream ifs(pathBuf);
    if (!ifs) {
        // ないなら空のままでOK（まだ未定義ステージとか）
        return;
    }

    // 1行目は "W,H,spawnTx,spawnTy" とかが来る設計だったよね
    std::string line;
    if (!std::getline(ifs, line)) return;
    // それ以降の行でタイルIDを読み込んで grid にセット
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
    blinkStrength_ = std::pow(basePulse, 3.0f);

    auto *in = engine_->input;
    // A / D でステージ番号変更
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

    // Enter で決定 -> GameScene(curStage_)
    if (in->IsKeyTriggered(DIK_RETURN) || in->IsKeyTriggered(DIK_NUMPADENTER)) {
        engine_->sceneManager->ChangeScene(std::make_unique<GameScene>(curStage_));
        return;
    }

    // Esc でタイトルに戻るとか欲しければここで
    if (in->IsKeyTriggered(DIK_ESCAPE)) {
        // タイトルに戻る
        // (TitleScene は引数無しでOK)
        // 注意: TitleSceneのヘッダが必要ならインクルードして
        extern IScene *CreateTitleScene(); // もしファクトリ関数を用意するなら
        // ここでは素直に直接:
        engine_->sceneManager->ChangeScene(std::make_unique<TitleScene>());
        return;
    }
}

// ===== 背景・夜景など（TitleScene::Drawの前半を凝縮） =====
void StageSelectScene::DrawBackgroundLayers_(float W, float H) {
    auto Lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // 1) 空
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

    // 2) ビル群
    auto DrawBuildings = [&](float z, float yBase, float span,
        float wMin, float wMax,
        float hMin, float hMax,
        float tiltDeg, bool warnLight) {
            int count = int(W / span) + 8;
            for (int i = -count / 2; i <= count / 2; ++i) {
                float rx = i * span;
                // 乱数っぽいやつ
                uint32_t seed = (uint32_t)(i * 73 + (int)(z * 10));
                float r = (float)(seed & 0xFFFF) / 65535.0f;

                float rw = Lerp(wMin, wMax, r);
                float rh = Lerp(hMin, hMax, 1.0f - r);

                DrawModel_(mdlSolid_,
                    {rx, yBase + rh * 0.5f, z},
                    {rw, rh * 0.5f, 0.22f},
                    {0,0,(i & 1) ? tiltDeg : -tiltDeg});

                DrawModel_(mdlSwitchOff_,
                    {rx + rw * 0.14f, yBase + rh + 0.12f, z - 0.03f},
                    {rw * 0.12f, rw * 0.12f, 0.18f},
                    {0,0,(i & 1) ? -6.0f : 8.0f});

                if (warnLight && ((i + (int)z) % 5 == 0)) {
                    DrawModel_(mdlSwitchOn_,
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

    // 3) クレーン／ワイヤ／吊り荷
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

    DrawModel_(mdlSwitch_,
        {W * 0.22f, H * 0.72f, 24.4f},
        {0.14f,   0.14f,    0.22f},
        {0,0,0});

    DrawModel_(mdlSolid_,
        {W * 0.22f, H * 0.55f, 24.3f},
        {0.35f, 0.08f, 0.25f},
        {0,0, 4.0f});

    DrawModel_(mdlSwitchOn_,
        {W * 0.22f, H * 0.47f, 24.2f},
        {0.15f, 0.08f, 0.22f},
        {0,0, 0.0f});

    // 4) 手すりなど前景
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

    // 5) 投光器（左右）
    auto Flood = [&](XMFLOAT3 b, float rotZ) {
        DrawModel_(mdlSolid_,
            {b.x, b.y, 22.0f},
            {0.05f, 0.55f, 0.25f},
            {0,0,0});

        DrawModel_(mdlSwitchOn_,
            {b.x, b.y + 0.38f, 21.9f},
            {0.22f, 0.12f, 0.22f},
            {0,0, rotZ});

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

// ===== 画面下に "STAGE XX" のバナーを出す =====
void StageSelectScene::DrawStageNumberBanner_(float W, float H) {
    // TitleScene::DrawSpaceSign_ に似たやつをちょっとシンプル化
    // バナーは中央下に鉄骨バー＋光帯で "STAGE N" っぽさを表現する

    float originY = H * (-0.10f); // 画面下寄り
    float zBase = -0.6f;
    float glyphW = W * 0.03f;
    float glyphH = H * 0.03f;
    float span = glyphW * 1.2f;

    // 点滅帯パラメータ
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

    // "STAGE" の文字は雑に棒組みでOK（TitleSceneのDrawSpaceSign_のS/A/Eベースでいい）
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
        // Cっぽい外周＋中にちょい棒
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

    // ステージ番号を一桁ずつ「デジタルっぽいAフレーム」で描く
    auto DrawDigit = [&](float gx, int digit) {
        // 簡易7セグ風（棒のON/OFF）
        struct Seg { float ox, oy, w, h, rz; bool on; };
        // 上, 中, 下, 左上, 左下, 右上, 右下 みたいに決める
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
            DrawBarBase(gx + ox, originY + oy, glyphW * w, glyphH * h, rz);
            DrawBarGlow(gx + ox, originY + oy, glyphW * w, glyphH * h * 0.4f, rz);
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

    // "STAGE"
    float startX = -span * 3.5f;
    Draw_S(startX + span * 0.0f);
    Draw_T(startX + span * 1.0f);
    Draw_A(startX + span * 2.0f);
    Draw_G(startX + span * 3.0f);
    Draw_E(startX + span * 4.0f);

    // 数字 (2桁想定)
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
void StageSelectScene::DrawPreviewMiniMap_(float W, float H) {
    (void)W;
    // 画面中央にちょっと小さくグリッドを並べる
    // 実際はGameScene::Draw のタイル描画ロジックの超縮小版
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    auto Deg = [](float d) { return XMConvertToRadians(d); };

    auto DrawOne = [&](Model &m, const XMFLOAT3 &pos, float zPushRot, bool crackOrDeco) {
        Transform t{};
        t.pos = pos;
        t.scale = {0.5f * 0.4f, 0.5f * 0.4f, 0.5f * 0.4f}; // 小さめ
        t.rot = {0,0,Deg(zPushRot)};
        mr->Draw(cmd, m, t);

        if (crackOrDeco) {
            Transform sign{};
            sign.pos = {pos.x, pos.y + 0.3f, pos.z - 0.05f};
            sign.scale = {0.2f, 0.15f, 0.2f};
            sign.rot = {0,0,Deg(12)};
            mr->Draw(cmd, mdlSolid_, sign);
        }
        };

    // プレビューを画面中央やや下に小さく表示する
    float baseZ = 5.0f; // カメラより奥だけど背景よりは手前
    float baseY = H * 0.25f;
    float baseX = 0.0f;
    float scaleTile = 0.4f; // 1タイルを0.4ワールドくらいに縮めて並べる

    for (int ty = 0; ty < kMapH; ++ty) {
        for (int tx = 0; tx < kMapW; ++tx) {
            Tile t = previewGrid_[ty][tx];

            // スイッチ系はON/OFF関係なくとりあえず見せる
            Model *m = nullptr;
            bool showCrack = false;

            switch (t) {
            case Tile::Solid: m = &mdlSolid_; break;
            case Tile::FragileAny: m = &mdlFragileAny_; showCrack = true; break;
            case Tile::FragileTop: m = &mdlFragileTop_; showCrack = true; break;
            case Tile::FragileBottom: m = &mdlFragileBottom_; showCrack = true; break;
            case Tile::Regen: m = &mdlRegen_; showCrack = true; break;
            case Tile::Spring: m = &mdlSpring_; break;
            case Tile::Spike: m = &mdlSpike_; break;
            case Tile::Switch: m = &mdlSwitch_; break;
            case Tile::SwitchBlockOn: m = &mdlSwitchOn_; break;
            case Tile::SwitchBlockOff: m = &mdlSwitchOff_; break;
            case Tile::JumpOnly: m = &mdlJumpOnly_; break;
            default: break;
            }
            if (!m) continue;

            float wx = xOffsetPreview_ + tx * kTile;
            float wy = (float)(kMapH - 1 - ty) * kTile;

            // 縮小＋オフセット
            float drawX = baseX + wx * scaleTile;
            float drawY = baseY + wy * scaleTile;
            float drawZ = baseZ;

            DrawOne(*m, {drawX, drawY, drawZ}, ((tx + ty) & 1) ? 4.0f : -4.0f, showCrack);
        }
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

    // プレビュー
    DrawPreviewMiniMap_(W, H);

    // 下の "STAGE XX"
    DrawStageNumberBanner_(W, H);

    mr->End(cmd);

    // ちょい説明UI（ImGui）
    ImGui::Begin("Select", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("A / D : Select Stage");
    ImGui::Text("Enter : Start");
    ImGui::Text("Esc   : Back to Title");
    ImGui::Text("Current: %d", curStage_);
    ImGui::End();
}

// ===== Finalize =====
void StageSelectScene::Finalize() {
    // ComPtr任せ
}
