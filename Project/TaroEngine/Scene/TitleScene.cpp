#define NOMINMAX
#include "TitleScene.h"

#include <cmath>
#include <algorithm>
#include <string>
#include <cstdio>

#include "DirectXCommon.h"
#include "ModelRenderer.h"
#include "BufferUtility.h"
#include "Input.h"
#include "SceneManager.h"
#include "StageSelectScene.h"

#include <DirectXTex/DirectXTex.h>
#include <DirectXTex/d3dx12.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ==== UTF-8 -> UTF-16 ユーティリティ ====
static std::wstring WidenU16_(const std::string &s) {
    if (s.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// ==== 0..1 の決定論ランダム ====
float TitleScene::Hash01_(int seed) {
    uint32_t h = (uint32_t)(seed) * 2654435761u;
    h ^= (h >> 13); h *= 0x5bd1e995u; h ^= (h >> 15);
    return (h & 0xFFFFFF) / float(0xFFFFFF);
}

// =======================================================
// Initialize
// =======================================================
void TitleScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engine_ = engine;
    render_ = render;

    auto *dx = engine_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // ---------- モデル読み込み ----------
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

    // ---------- テクスチャSRVをバインド ----------
    auto setupTex = [&](Model &m, UINT slot, ComPtr<ID3D12Resource> &holder) {
        if (m.GetAlbedoPath().empty()) return;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        if (LoadTextureSRV_(WidenU16_(m.GetAlbedoPath()), slot, holder, gpu)) {
            m.SetAlbedoSRV(gpu);
        }
        };

    // SRVスロットは空いてる番号でいい。0～10でOK。
    setupTex(mdlSolid_, 0, texSolid_);
    setupTex(mdlFragileAny_, 1, texFragileAny_);
    setupTex(mdlFragileTop_, 2, texFragileTop_);
    setupTex(mdlFragileBottom_, 3, texFragileBottom_);
    setupTex(mdlRegen_, 4, texRegen_);
    setupTex(mdlSpring_, 5, texSpring_);
    setupTex(mdlSpike_, 6, texSpike_);
    setupTex(mdlSwitch_, 7, texSwitch_);
    setupTex(mdlSwitchBlockOn_, 8, texSwitchOn_);
    setupTex(mdlSwitchBlockOff_, 9, texSwitchOff_);
    setupTex(mdlJumpOnly_, 10, texJumpOnly_);


    // ---------- ワールドスケールとカメラ ----------
    worldW_ = 32.0f;
    worldH_ = 18.0f;

    spawnTopY_ = worldH_ + 2.0f;   // 画面より上から出す
    despawnY_ = -4.0f;            // ここより下に行ったら消す
    spawnLeftX_ = -worldW_ * 0.3f;
    spawnRightX_ = worldW_ * 0.3f;

    float screenW = (float)dx->GetWidth();
    float screenH = (float)dx->GetHeight();
    float aspect = screenW / std::max(1.0f, screenH);
    float sceneAspect = worldW_ / worldH_;

    float orthoW, orthoH;
    if (aspect >= sceneAspect) {
        orthoH = worldH_;
        orthoW = worldH_ * aspect;
    } else {
        orthoW = worldW_;
        orthoH = worldW_ / aspect;
    }

    float camZ = -50.0f;
    float centerX = 0.0f;
    float centerY = worldH_ * 0.5f; // 9くらい

    camera_.Initialize(
        {centerX, centerY, camZ},
        {centerX, centerY, 0.0f},
        60.0f,
        aspect,
        0.1f,
        1000.0f
    );
    camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);
    camera_.LookAt(
        {centerX, centerY, camZ},
        {centerX, centerY, 0.0f}
    );
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    // ---------- スポーン管理 ----------
    spawnTimer_ = 0.0f;
    spawnInterval_ = 0.1f;

    // 全ブロックを初期化
    for (int i = 0; i < kMaxBlocks_; ++i) {
        blocks_[i] = FallingBlock{};
        blocks_[i].alive = false;
    }
}

// =======================================================
// Finalize
// =======================================================
void TitleScene::Finalize() {
    // ComPtr任せ
}

// =======================================================
// RefreshCameraOrtho_ : リサイズ追従
// =======================================================
void TitleScene::RefreshCameraOrtho_() {
    auto *dx = engine_->directXCommon;
    float w = (float)dx->GetWidth();
    float h = (float)dx->GetHeight();
    if (w <= 0 || h <= 0) return;

    float aspect = w / std::max(1.0f, h);
    float sceneAspect = worldW_ / worldH_;

    float orthoW, orthoH;
    if (aspect >= sceneAspect) {
        orthoH = worldH_;
        orthoW = worldH_ * aspect;
    } else {
        orthoW = worldW_;
        orthoH = worldW_ / aspect;
    }

    if (camera_.IsOrtho()) {
        camera_.SetOrthoViewSize(orthoW, orthoH);
    }
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
}

// =======================================================
// SpawnOne_ : 新しい落下ブロックを1つ有効化
// =======================================================
void TitleScene::SpawnOne_() {
    int slot = -1;
    for (int i = 0; i < kMaxBlocks_; ++i) {
        if (!blocks_[i].alive) { slot = i; break; }
    }
    if (slot < 0) return;

    float r0 = Hash01_(slot * 37 + 11);
    float r1 = Hash01_(slot * 53 + 7);
    float r2 = Hash01_(slot * 97 + 19);

    FallingBlock &b = blocks_[slot];
    b.alive = true;
    b.t = 0.0f;

    // 出現位置（Xはランダム、Yは画面の上外）
    float spawnX = spawnLeftX_ + (spawnRightX_ - spawnLeftX_) * r0;
    b.baseX = spawnX;
    b.pos = {spawnX, spawnTopY_, -1.0f};

    // 落下スピード / 横揺れ設定
    b.fallSpeed = 5.0f + r1 * 2.0f;
    b.swayAmp = 2.0f + r2 * 1.5f;
    b.swayFreq = 0.6f + r1 * 0.4f;
    b.phase = r2 * 6.28318f;

    // モデルのスケールは固定（モデルの素の形をそのまま見せたいなら等倍演出にしておく）
    b.w = 1.0f;
    b.h = 1.0f;
    b.d = 1.0f;

    // ちょい回転スタートで漂わせる
    b.rotZDeg = (r1 * 2.0f - 1.0f) * 10.0f;

    // ★ここ：11種を順番に出す
    b.kind = nextKindIndex_ % 11;
    nextKindIndex_++;
}


// =======================================================
// UpdateDebris_ : 生きてるブロックを全員更新
// =======================================================
void TitleScene::UpdateDebris_(float dt) {
    for (int i = 0; i < kMaxBlocks_; ++i) {
        FallingBlock &b = blocks_[i];
        if (!b.alive) continue;

        b.t += dt;

        // 下方向に等速落下（画面を通過）
        b.pos.y -= b.fallSpeed * dt;

        // 横方向はサインでゆったりスイング
        float sway = std::sinf(b.t * b.swayFreq + b.phase) * b.swayAmp;
        b.pos.x = b.baseX + sway;

        // ゆっくり回転させるとさらに漂ってる感が出る
        b.rotZDeg += dt * 15.0f; // 1秒で15度くらい

        // 画面外(十分下)まで行ったら消す
        if (b.pos.y < despawnY_) {
            b.alive = false;
        }
    }
}

// =======================================================
// DrawDebris_ : 落下中ブロックたちを描画
// =======================================================
void TitleScene::DrawDebris_() {
    for (int i = 0; i < kMaxBlocks_; ++i) {
        const FallingBlock &b = blocks_[i];
        if (!b.alive) continue;

        Model *useModel = nullptr;
        switch (b.kind) {
        case 0:  useModel = &mdlSolid_;            break;
        case 1:  useModel = &mdlFragileAny_;       break;
        case 2:  useModel = &mdlFragileTop_;       break;
        case 3:  useModel = &mdlFragileBottom_;    break;
        case 4:  useModel = &mdlRegen_;            break;
        case 5:  useModel = &mdlSpring_;           break;
        case 6:  useModel = &mdlSpike_;            break;
        case 7:  useModel = &mdlSwitch_;           break;
        case 8:  useModel = &mdlSwitchBlockOn_;    break;
        case 9:  useModel = &mdlSwitchBlockOff_;   break;
        case 10: useModel = &mdlJumpOnly_;         break;
        default: useModel = &mdlSolid_;            break;
        }

        DrawModel_(*useModel,
            b.pos,
            {b.w, b.h, b.d},
            {0.0f, 0.0f, b.rotZDeg},
            1.0f);
    }
}


// =======================================================
// DrawBackground_ : 背景一枚板（水色にしたいならそのテクスチャを水色に）
// =======================================================
void TitleScene::DrawBackground_() {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    auto Deg = [](float d) { return XMConvertToRadians(d); };

    Transform t{};
    t.pos = {0.0f, worldH_ * 0.5f, 40.0f}; // カメラより奥
    t.scale = {worldW_ * 3.0f * 0.5f,
               worldH_ * 3.0f * 0.5f,
               0.5f * 0.5f};
    t.rot = {0,0,Deg(0.0f)};

    // ただの板をでっかく
    mr->Draw(cmd, mdlSolid_, t, 1.0f);
}

// =======================================================
// Update : カメラ追従・スポーン制御・入力
// =======================================================
void TitleScene::Update(float dt) {
    RefreshCameraOrtho_();

    // スポーンタイマ進行
    spawnTimer_ += dt;
    if (spawnTimer_ >= spawnInterval_) {
        spawnTimer_ = 0.0f;
        SpawnOne_();
    }

    // ブロック更新
    UpdateDebris_(dt);

    // スペースでシーン遷移
    if (engine_->input && engine_->input->IsKeyTriggered(DIK_SPACE)) {
        engine_->sceneManager->ChangeScene(std::make_unique<StageSelectScene>());
    }
}

// =======================================================
// Draw
// =======================================================
void TitleScene::Draw() {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    mr->Begin(cmd, dx, camera_);

    // 1) 背景
    DrawBackground_();
    // 2) 落ちてるブロックたち
    DrawDebris_();

    mr->End(cmd);
}

// =======================================================
// DrawModel_ : ModelRenderer 経由で描画
// =======================================================
void TitleScene::DrawModel_(
    Model &m,
    const XMFLOAT3 &pos,
    const XMFLOAT3 &fullScale,
    const XMFLOAT3 &rotDeg,
    float alphaMul) {

    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    auto Deg = [](float d) { return XMConvertToRadians(d); };

    Transform tr{};
    tr.pos = pos;
    tr.scale = {
        fullScale.x * 0.5f,
        fullScale.y * 0.5f,
        fullScale.z * 0.5f
    };
    tr.rot = {
        Deg(rotDeg.x),
        Deg(rotDeg.y),
        Deg(rotDeg.z)
    };

    mr->Draw(cmd, m, tr, alphaMul);
}

// =======================================================
// LoadTextureSRV_ : テクスチャを読み込んでSRVを作る
// =======================================================
bool TitleScene::LoadTextureSRV_(
    const std::wstring &fileU16, UINT srvIndex,
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

    const DirectX::TexMetadata &meta = img.GetMetadata();
    DXGI_FORMAT dstFmt = meta.format;

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
    dstFmt = useMeta.format;

    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = (UINT)useMeta.width;
    texDesc.Height = (UINT)useMeta.height;
    texDesc.DepthOrArraySize = (UINT16)useMeta.arraySize;
    texDesc.MipLevels = (UINT16)(useMeta.mipLevels ? useMeta.mipLevels : 1);
    texDesc.Format = dstFmt;
    texDesc.SampleDesc = {1,0};
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
        IID_PPV_ARGS(&outTex)
    );
    if (FAILED(hr)) return false;

    UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);
    ComPtr<ID3D12Resource> upload = BufferUtility::CreateUploadBuffer(device, uploadSize);

    // 小さな一時コマンドリストを作って転送
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

    // サブリソースコピー
    {
        std::vector<D3D12_SUBRESOURCE_DATA> subs((size_t)useMeta.mipLevels);
        for (size_t m = 0; m < useMeta.mipLevels; ++m) {
            const DirectX::Image &im = srcImgs[m];
            subs[m].pData = im.pixels;
            subs[m].RowPitch = im.rowPitch;
            subs[m].SlicePitch = im.slicePitch;
        }

        UpdateSubresources(
            list.Get(), outTex.Get(), upload.Get(),
            0, 0, (UINT)useMeta.mipLevels,
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

    // フェンスで待つ
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
    ID3D12DescriptorHeap *srvHeap = engine_->directXCommon->GetSrvHeap();
    UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += SIZE_T(inc) * srvIndex;

    D3D12_GPU_DESCRIPTOR_HANDLE gpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(inc) * srvIndex;
    outGpuHandle = gpu;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = dstFmt;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

    device->CreateShaderResourceView(outTex.Get(), &srvDesc, cpu);

    return true;
}
