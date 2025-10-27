#define NOMINMAX
#include "TitleScene.h"

#include <algorithm>
#include <vector>
#include <string>
#include <cassert>

#include "DirectXCommon.h"
#include "ModelRenderer.h"
#include "BufferUtility.h"
#include <DirectXTex/DirectXTex.h>
#include <DirectXTex/d3dx12.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// UTF-8 → UTF-16
static std::wstring Widen(const std::string &s) {
    if (s.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// ちょい乱数（決定性あり）
static float Hash01(int n) {
    uint32_t h = (uint32_t)(n) * 2654435761u;
    h ^= (h >> 13); h *= 0x5bd1e995u; h ^= (h >> 15);
    return (h & 0xFFFFFF) / float(0xFFFFFF);
}

void TitleScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engine_ = engine;
    render_ = render;

    auto *dx = engine_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // === モデル読込（Block ディレクトリの既存ファイル名に一致） ===
    mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");
    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");
    mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");
    mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
    mdlSwitch_.Initialize(device, "Resources/Model/Block/switch.obj");
    mdlSwitchOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");

    // === テクスチャSRV割り当て（OBJの map_Kd をそのまま SRV 化） ===
    auto setupTex = [&](Model &m, UINT slot, ComPtr<ID3D12Resource> &holder) {
        if (m.GetAlbedoPath().empty()) return;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        if (LoadTextureSRV_(Widen(m.GetAlbedoPath()), slot, holder, gpu)) {
            m.SetAlbedoSRV(gpu);
        }
        };
    setupTex(mdlSolid_, kSrv_T_Solid, texSolid_);
    setupTex(mdlJumpOnly_, kSrv_T_JumpOnly, texJumpOnly_);
    setupTex(mdlSpike_, kSrv_T_Spike, texSpike_);
    setupTex(mdlSpring_, kSrv_T_Spring, texSpring_);
    setupTex(mdlSwitch_, kSrv_T_Switch, texSwitch_);
    setupTex(mdlSwitchOn_, kSrv_T_SwitchOn, texSwitchOn_);
    setupTex(mdlSwitchOff_, kSrv_T_SwitchOff, texSwitchOff_);

    // === カメラ（ゲームシーンと同条件） ===
    float w = (float)dx->GetWidth();
    float h = (float)dx->GetHeight();
    float aspect = std::max(1.0f, w) / std::max(1.0f, h);
    float orthoW = virtualWorldH_ * aspect;
    float orthoH = virtualWorldH_;

    camera_.Initialize({0.0f, orthoH * 0.5f, -50.0f}, {0,0,0}, 60.0f, aspect, 0.1f, 1000.0f);
    camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);
    camera_.LookAt({0.0f, orthoH * 0.5f, -50.0f}, {0.0f, orthoH * 0.5f, 0.0f});
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
}

void TitleScene::Finalize() {
    // ComPtr による自動解放
}

void TitleScene::RefreshCameraOrtho_() {
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

void TitleScene::Update(float /*dt*/) {
    RefreshCameraOrtho_();
}

// ★ ここが重要：OBJは“2x2x2（中心原点）”基準のものが多いので、
//   fullScale（幅・高さ・奥行き）→半サイズへ変換して渡す。
void TitleScene::DrawModel_(Model &m,
    const XMFLOAT3 &pos,
    const XMFLOAT3 &fullScale,
    const XMFLOAT3 &rotDeg) {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    auto Deg = [](float d) { return XMConvertToRadians(d); };

    Transform t{};
    t.pos = pos;
    t.scale = {fullScale.x * 0.5f, fullScale.y * 0.5f, fullScale.z * 0.5f}; // ←半径化
    t.rot = {Deg(rotDeg.x), Deg(rotDeg.y), Deg(rotDeg.z)};

    mr->Draw(cmd, m, t);
}

void TitleScene::Draw() {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    mr->Begin(cmd, dx, camera_);

    float aspect = std::max(1.0f, (float)dx->GetWidth()) / std::max(1.0f, (float)dx->GetHeight());
    float W = virtualWorldH_ * aspect;
    float H = virtualWorldH_;

    auto Lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto Deg = [](float d) { return XMConvertToRadians(d); };

    // =========================
    // 1) 空：2枚の大板で擬似グラデーション
    // =========================
    DrawModel_(mdlSpring_, {0.0f, H * 0.50f, 38.0f}, {W * 2.6f, H * 2.2f, 0.25f}, {0,0,0});
    DrawModel_(mdlJumpOnly_, {0.0f, H * 0.48f, 37.8f}, {W * 2.6f, H * 1.9f, 0.25f}, {0,0,-4.0f});

    // 薄い雲（横長板を数枚）
    for (int i = -3; i <= 3; ++i) {
        float rx = i * (W * 0.35f);
        float ry = H * (0.70f + 0.03f * std::sin(i * 0.9f));
        float rw = W * Lerp(0.50f, 0.80f, Hash01(i * 31));
        float rh = H * 0.06f;
        float rz = 37.6f - (i & 1) * 0.2f;
        DrawModel_(mdlSpring_, {rx, ry, rz}, {rw, rh, 0.05f}, {0, 0, (i & 1) ? -8.0f : 10.0f});
    }

    // =========================
    // 2) ビル群（遠・中・近）
    // =========================
    auto DrawBuildings = [&](float z, float yBase, float span,
        float wMin, float wMax, float hMin, float hMax,
        float tilt, bool warnLight) {
            int count = int(W / span) + 8;
            for (int i = -count / 2; i <= count / 2; ++i) {
                float rx = i * span;
                float r = Hash01(i * 73 + (int)(z * 10));
                float rw = Lerp(wMin, wMax, r);
                float rh = Lerp(hMin, hMax, 1.0f - r);

                // 本体
                DrawModel_(mdlSolid_, {rx, yBase + rh * 0.5f, z},
                    {rw, rh * 0.5f, 0.22f},
                    {0, 0, (i & 1) ? tilt : -tilt});

                // 屋上構造
                DrawModel_(mdlSwitchOff_,
                    {rx + rw * 0.14f, yBase + rh + 0.12f, z - 0.03f},
                    {rw * 0.12f, rw * 0.12f, 0.18f},
                    {0,0,(i & 1) ? -6.0f : 8.0f});

                // 警告灯
                if (warnLight && ((i + (int)z) % 5 == 0)) {
                    DrawModel_(mdlSwitchOn_,
                        {rx, yBase + rh + 0.24f, z - 0.05f},
                        {0.10f, 0.10f, 0.15f},
                        {0,0,0});
                }
            }
        };

    DrawBuildings(33.0f, H * 0.06f, W * 0.14f, W * 0.06f, W * 0.10f, H * 0.16f, H * 0.30f, 2.0f, true);
    DrawBuildings(29.0f, H * 0.08f, W * 0.12f, W * 0.07f, W * 0.12f, H * 0.18f, H * 0.36f, 3.0f, true);
    DrawBuildings(25.0f, H * 0.10f, W * 0.10f, W * 0.08f, W * 0.14f, H * 0.22f, H * 0.40f, 4.0f, false);

    // =========================
    // 3) クレーン／ワイヤ／吊り荷
    // =========================
    DrawModel_(mdlJumpOnly_, {-W * 0.32f, H * 0.86f, 24.8f}, {0.06f, H * 0.55f, 0.30f}, {0,0,0});
    DrawModel_(mdlSolid_, {-W * 0.06f, H * 1.03f, 24.6f}, {W * 0.55f, 0.06f, 0.30f}, {0,0,-9.0f});
    DrawModel_(mdlSolid_, {W * 0.22f, H * 0.88f, 24.5f}, {0.035f,  H * 0.28f, 0.25f}, {0,0,0});
    DrawModel_(mdlSwitch_, {W * 0.22f, H * 0.72f, 24.4f}, {0.14f,   0.14f,    0.22f}, {0,0,0});

    // 吊り荷（鉄骨）
    DrawModel_(mdlSolid_, {W * 0.22f, H * 0.55f, 24.3f}, {0.35f, 0.08f, 0.25f}, {0,0, 4.0f});
    // 作業灯
    DrawModel_(mdlSwitchOn_, {W * 0.22f, H * 0.47f, 24.2f}, {0.15f, 0.08f, 0.22f}, {0,0, 0.0f});

    // =========================
    // 4) 前景：手すり／補強材／ケーブル
    // =========================
    {
        float railY = -0.6f;
        DrawModel_(mdlSolid_, {0.0f, railY, -0.40f}, {W * 0.66f, 0.05f, 0.22f}, {0,0,0});

        for (int i = -3; i <= 3; ++i) {
            float x = i * (W * 0.16f);
            DrawModel_(mdlSolid_, {x, railY + 0.20f, -0.41f},
                {W * 0.09f, 0.03f, 0.22f},
                {0, 0, (i % 2 == 0) ? -10.0f : 12.0f});
        }
        // ケーブル（薄い板）
        for (int i = -2; i <= 2; ++i) {
            DrawModel_(mdlJumpOnly_, {i * (W * 0.18f), railY + 0.55f, -0.42f},
                {W * 0.08f, 0.01f, 0.2f},
                {0,0, 10.0f * std::sinf(float(i))});
        }
    }

    // =========================
    // 5) 投光器（左右）
    // =========================
    auto Flood = [&](XMFLOAT3 b, float rotZ) {
        DrawModel_(mdlSolid_, {b.x, b.y, 22.0f}, {0.05f, 0.55f, 0.25f}, {0,0,0});
        DrawModel_(mdlSwitchOn_, {b.x, b.y + 0.38f, 21.9f}, {0.22f, 0.12f, 0.22f}, {0,0, rotZ});
        DrawModel_(mdlSpike_, {b.x + 0.10f, b.y + 0.26f, 21.7f}, {W * 0.22f, H * 0.10f, 0.05f}, {0,0, rotZ - 12.0f});
        DrawModel_(mdlSpike_, {b.x + 0.08f, b.y + 0.28f, 21.6f}, {W * 0.24f, H * 0.11f, 0.05f}, {0,0, rotZ - 14.0f});
        };
    Flood({-W * 0.48f, H * 0.82f, 0}, 10.0f);
    Flood({W * 0.52f, H * 0.74f, 0}, 18.0f);

    mr->End(cmd);
}

// ===== SRV作成（GameScene簡略版） =====
bool TitleScene::LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
    ComPtr<ID3D12Resource> &outTex,
    D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle) {
    auto *dx = engine_->directXCommon;
    ID3D12Device *device = dx->GetDevice();
    if (!device || fileU16.empty()) return false;

    DirectX::ScratchImage img, conv;
    HRESULT hr = DirectX::LoadFromWICFile(fileU16.c_str(),
        DirectX::WIC_FLAGS_FORCE_SRGB,
        nullptr, img);
    if (FAILED(hr)) return false;

    const auto &meta = img.GetMetadata();
    DXGI_FORMAT fmt = meta.format;

    if (!DirectX::IsCompressed(meta.format) && !DirectX::IsSRGB(meta.format)) {
        hr = DirectX::Convert(img.GetImages(), img.GetImageCount(), meta,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DirectX::TEX_FILTER_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT, conv);
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

    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&outTex));
    if (FAILED(hr)) return false;

    UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);
    ComPtr<ID3D12Resource> upload = BufferUtility::CreateUploadBuffer(device, uploadSize);

    // one-shot
    ComPtr<ID3D12CommandQueue> queue;
    {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    }
    ComPtr<ID3D12CommandAllocator> alloc;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    ComPtr<ID3D12GraphicsCommandList> list;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&list));

    std::vector<D3D12_SUBRESOURCE_DATA> subs((size_t)useMeta.mipLevels);
    for (size_t m = 0; m < useMeta.mipLevels; ++m) {
        const DirectX::Image &im = src[m];
        subs[m].pData = im.pixels;
        subs[m].RowPitch = im.rowPitch;
        subs[m].SlicePitch = im.slicePitch;
    }
    UpdateSubresources(list.Get(), outTex.Get(), upload.Get(), 0, 0, (UINT)useMeta.mipLevels, subs.data());
    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(outTex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->ResourceBarrier(1, &toSRV);
    list->Close();
    ID3D12CommandList *lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence; device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    queue->Signal(fence.Get(), 1);
    if (fence->GetCompletedValue() < 1) { fence->SetEventOnCompletion(1, evt); WaitForSingleObject(evt, INFINITE); }
    CloseHandle(evt);

    // SRV
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
