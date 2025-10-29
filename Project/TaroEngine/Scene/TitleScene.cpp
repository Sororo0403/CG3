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

namespace {
    // タイトル画面用SRV開始スロット。
    // ImGuiなどが0番台を使ってるなら被らないように後ろへずらす。
    constexpr UINT kTitleSrvBase = 32;
}

// ==== UTF-8 -> UTF-16 ====
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

    // スイッチ本体（押すギミック）
    mdlSwitchOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");

    // スイッチ連動床（ON時に出る床 / OFF時に出る床）
    mdlSwitchBlockOn_.Initialize(device, "Resources/Model/Block/switchblock_on.obj");
    mdlSwitchBlockOff_.Initialize(device, "Resources/Model/Block/switchblock_off.obj");

    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");

    // ---------- テクスチャSRVをバインド ----------
    auto setupTex = [&](Model &m, UINT slot, ComPtr<ID3D12Resource> &holder) {
        if (m.GetAlbedoPath().empty()) return;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        if (LoadTextureSRV_(WidenU16_(m.GetAlbedoPath()), slot, holder, gpu)) {
            m.SetAlbedoSRV(gpu);
        }
        };

    // SRVスロット割り当て
    setupTex(mdlSolid_, kTitleSrvBase + 0, texSolid_);
    setupTex(mdlFragileAny_, kTitleSrvBase + 1, texFragileAny_);
    setupTex(mdlFragileTop_, kTitleSrvBase + 2, texFragileTop_);
    setupTex(mdlFragileBottom_, kTitleSrvBase + 3, texFragileBottom_);
    setupTex(mdlRegen_, kTitleSrvBase + 4, texRegen_);
    setupTex(mdlSpring_, kTitleSrvBase + 5, texSpring_);
    setupTex(mdlSpike_, kTitleSrvBase + 6, texSpike_);

    // スイッチ本体ON/OFF
    setupTex(mdlSwitchOn_, kTitleSrvBase + 7, texSwitchOn_);
    setupTex(mdlSwitchOff_, kTitleSrvBase + 8, texSwitchOff_);

    // スイッチ連動床ON/OFF
    setupTex(mdlSwitchBlockOn_, kTitleSrvBase + 9, texSwitchBlockOn_);
    setupTex(mdlSwitchBlockOff_, kTitleSrvBase + 10, texSwitchBlockOff_);

    setupTex(mdlJumpOnly_, kTitleSrvBase + 11, texJumpOnly_);

    // ---------- ワールドスケールとカメラ ----------
    // ゲーム内ステージと似たスケールで見せたい
    worldW_ = 32.0f;
    worldH_ = 18.0f;

    spawnTopY_ = worldH_ + 2.0f; // 画面上ちょい外から降らせる
    despawnY_ = -4.0f;
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
    float centerY = worldH_ * 0.5f;

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

    // ---------- スポーン状態 ----------
    spawnTimer_ = 0.0f;
    spawnInterval_ = 0.1f; // 0.1秒ごとに落とす感じ
    nextKindIndex_ = 0;

    // ブロックリスト初期化
    for (int i = 0; i < kMaxBlocks_; ++i) {
        blocks_[i] = FallingBlock{};
        blocks_[i].alive = false;
    }
}

// =======================================================
// Finalize
// =======================================================
void TitleScene::Finalize() {
    // ComPtr が勝手に解放してくれるので何もしない
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
    // 空いてるスロットを探す
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

    // 出現位置
    float spawnX = spawnLeftX_ + (spawnRightX_ - spawnLeftX_) * r0;
    b.baseX = spawnX;
    b.pos = {spawnX, spawnTopY_, 0.0f};

    // 落下モーション
    b.fallSpeed = 5.0f + r1 * 2.0f;
    b.swayAmp = 2.0f + r2 * 1.5f;
    b.swayFreq = 0.6f + r1 * 0.4f;
    b.phase = r2 * 6.28318f;

    // サイズと回転は固定
    b.w = 1.0f;
    b.h = 1.0f;
    b.d = 1.0f;
    b.rotZDeg = 0.0f;

    // 種類は 0..10 を順番ローテ
    // 0:Solid
    // 1:FragileAny
    // 2:FragileTop
    // 3:FragileBottom
    // 4:Regen
    // 5:Spring
    // 6:Spike
    // 7:SwitchOn
    // 8:SwitchOff
    // 9:SwitchBlockOn
    //10:SwitchBlockOff
    // (JumpOnly はこのローテに入れたいなら増やしてもOK。
    //  今回は JumpOnly を SwitchBlockOff と差し替えず、下で分岐で扱うなら拡張する。
    //  とりあえず 0..10 の11種類に JumpOnly も含めたいなら下のマッピングで扱う)
    b.kind = nextKindIndex_ % 11;
    nextKindIndex_++;
}

// =======================================================
// UpdateDebris_ : 生きてるブロックの全更新
// =======================================================
void TitleScene::UpdateDebris_(float dt) {
    for (int i = 0; i < kMaxBlocks_; ++i) {
        FallingBlock &b = blocks_[i];
        if (!b.alive) continue;

        b.t += dt;

        // 重力っぽい落下（等速）
        b.pos.y -= b.fallSpeed * dt;

        // 横方向にフラフラ
        float sway = std::sinf(b.t * b.swayFreq + b.phase) * b.swayAmp;
        b.pos.x = b.baseX + sway;

        // 画面のかなり下まで落ちたら消す
        if (b.pos.y < despawnY_) {
            b.alive = false;
        }
    }
}

// =======================================================
// DrawDebris_ : 落下ブロック描画
// =======================================================
void TitleScene::DrawDebris_() {
    for (int i = 0; i < kMaxBlocks_; ++i) {
        const FallingBlock &b = blocks_[i];
        if (!b.alive) continue;

        Model *useModel = nullptr;
        switch (b.kind) {
        case 0:  useModel = &mdlSolid_;           break;
        case 1:  useModel = &mdlFragileAny_;      break;
        case 2:  useModel = &mdlFragileTop_;      break;
        case 3:  useModel = &mdlFragileBottom_;   break;
        case 4:  useModel = &mdlRegen_;           break;
        case 5:  useModel = &mdlSpring_;          break;
        case 6:  useModel = &mdlSpike_;           break;
        case 7:  useModel = &mdlSwitchOn_;        break; // スイッチ本体(ON)
        case 8:  useModel = &mdlSwitchOff_;       break; // スイッチ本体(OFF)
        case 9:  useModel = &mdlSwitchBlockOn_;   break; // スイッチ床ON
        case 10: useModel = &mdlSwitchBlockOff_;  break; // スイッチ床OFF
        default: useModel = &mdlJumpOnly_;        break; // 念のためfallback
        }

        DrawModel_(
            *useModel,
            b.pos,
            {b.w, b.h, b.d},
            {0.0f, 0.0f, b.rotZDeg},
            1.0f
        );
    }
}

// =======================================================
// Update : カメラ追従・スポーン制御・入力
// =======================================================
void TitleScene::Update(float dt) {
    RefreshCameraOrtho_();

    // 一定間隔で新しいブロックを落とす
    spawnTimer_ += dt;
    if (spawnTimer_ >= spawnInterval_) {
        spawnTimer_ = 0.0f;
        SpawnOne_();
    }

    // ブロック挙動更新
    UpdateDebris_(dt);

    // スペースでステージセレクトへ
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

    // 1) 背景（夜の工事現場レイヤ）
    DrawTitleBackground_();

    // 2) タイトル演出の降ってくるブロック達（手前に描く）
    DrawDebris_();

    mr->End(cmd);
}

// =======================================================
// DrawModel_ : 1モデル描画
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
// LoadTextureSRV_ : TitleScene版（GameSceneとほぼ同じ）
// =======================================================
bool TitleScene::LoadTextureSRV_(
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

    // 一時コマンドでテクスチャ転送
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

    // フェンス待ち
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

void TitleScene::DrawTitleBackground_() {
    // StageSelectScene::DrawBackgroundLayers_ をベースにしたやつ
    // ここでは worldW_ / worldH_ を使って同じノリの工事夜景を描く

    float W = worldW_;
    float H = worldH_;

    auto Lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // 1) 空っぽい奥行きレイヤ（夜空と薄い色ムラ）
    DrawModel_(
        mdlSpring_,                     // 空っぽい面として流用
        {0.0f, H * 0.50f, 38.0f},
        {W * 2.6f, H * 2.2f, 0.25f},
        {0,0,0},
        1.0f
    );

    DrawModel_(
        mdlJumpOnly_,                   // ちょい違う色で重ねる
        {0.0f, H * 0.48f, 37.8f},
        {W * 2.6f, H * 1.9f, 0.25f},
        {0,0,-4.0f},
        1.0f
    );

    for (int i = -3; i <= 3; ++i) {
        float rx = i * (W * 0.35f);
        float ry = H * (0.70f + 0.03f * std::sin(i * 0.9f));
        // 擬似ランダム幅
        float randv = float(((i * 31 * 2654435761u) & 255)) / 255.0f;
        float rw = W * Lerp(0.50f, 0.80f, randv);
        float rh = H * 0.06f;
        float rz = 37.6f - (i & 1) * 0.2f;

        DrawModel_(
            mdlSpring_,
            {rx, ry, rz},
            {rw, rh, 0.05f},
            {0,0,(i & 1) ? -8.0f : 10.0f},
            1.0f
        );
    }

    // 2) ビル群
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

                // ビル本体
                DrawModel_(
                    mdlSolid_,
                    {rx, yBase + rh * 0.5f, z},
                    {rw, rh * 0.5f, 0.22f},
                    {0,0,(i & 1) ? tiltDeg : -tiltDeg},
                    1.0f
                );

                // 屋上の機材箱（SwitchBlockOffでアクセント）
                DrawModel_(
                    mdlSwitchBlockOff_,
                    {rx + rw * 0.14f, yBase + rh + 0.12f, z - 0.03f},
                    {rw * 0.12f, rw * 0.12f, 0.18f},
                    {0,0,(i & 1) ? -6.0f : 8.0f},
                    1.0f
                );

                // 警告灯っぽい点（SwitchBlockOn）
                if (warnLight && ((i + (int)z) % 5 == 0)) {
                    DrawModel_(
                        mdlSwitchBlockOn_,
                        {rx, yBase + rh + 0.24f, z - 0.05f},
                        {0.10f, 0.10f, 0.15f},
                        {0,0,0},
                        1.0f
                    );
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

    // 3) クレーン・吊り荷・ライト
    // 支柱
    DrawModel_(
        mdlJumpOnly_,
        {-W * 0.32f, H * 0.86f, 24.8f},
        {0.06f, H * 0.55f, 0.30f},
        {0,0,0},
        1.0f
    );

    // アーム
    DrawModel_(
        mdlSolid_,
        {-W * 0.06f, H * 1.03f, 24.6f},
        {W * 0.55f, 0.06f, 0.30f},
        {0,0,-9.0f},
        1.0f
    );

    // 縦フレーム
    DrawModel_(
        mdlSolid_,
        {W * 0.22f, H * 0.88f, 24.5f},
        {0.035f,  H * 0.28f, 0.25f},
        {0,0,0},
        1.0f
    );

    // クレーン操作盤はスイッチOFFの箱っぽさで
    DrawModel_(
        mdlSwitchOff_,
        {W * 0.22f, H * 0.72f, 24.4f},
        {0.14f, 0.14f, 0.22f},
        {0,0,0},
        1.0f
    );

    // 吊り荷
    DrawModel_(
        mdlSolid_,
        {W * 0.22f, H * 0.55f, 24.3f},
        {0.35f, 0.08f, 0.25f},
        {0,0,4.0f},
        1.0f
    );

    // 投光器ヘッド（SwitchOnで光ってる感じ）
    DrawModel_(
        mdlSwitchOn_,
        {W * 0.22f, H * 0.47f, 24.2f},
        {0.15f, 0.08f, 0.22f},
        {0,0,0},
        1.0f
    );

    // 4) 手前手すり・注意テープ
    {
        float railY = -0.6f;

        // メイン柵
        DrawModel_(
            mdlSolid_,
            {0.0f, railY, -0.40f},
            {W * 0.66f, 0.05f, 0.22f},
            {0,0,0},
            1.0f
        );

        // 手すりの繰り返し板
        for (int i = -3; i <= 3; ++i) {
            float x = i * (W * 0.16f);
            DrawModel_(
                mdlSolid_,
                {x, railY + 0.20f, -0.41f},
                {W * 0.09f, 0.03f, 0.22f},
                {0,0,(i % 2 == 0) ? -10.0f : 12.0f},
                1.0f
            );
        }

        // 黄色テープ風の細い板（JumpOnlyモデルの細切れ）
        for (int i = -2; i <= 2; ++i) {
            DrawModel_(
                mdlJumpOnly_,
                {i * (W * 0.18f), railY + 0.55f, -0.42f},
                {W * 0.08f, 0.01f, 0.2f},
                {0,0, 10.0f * std::sinf(float(i))},
                1.0f
            );
        }
    }

    // 5) 両サイド投光器 (ポール＋ライト＋注意サイン板)
    auto Flood = [&](XMFLOAT3 b, float rotZDeg) {
        // ポール
        DrawModel_(
            mdlSolid_,
            {b.x, b.y, 22.0f},
            {0.05f, 0.55f, 0.25f},
            {0,0,0},
            1.0f
        );

        // ライトヘッド（SwitchOnで明るい想定）
        DrawModel_(
            mdlSwitchOn_,
            {b.x, b.y + 0.38f, 21.9f},
            {0.22f, 0.12f, 0.22f},
            {0,0, rotZDeg},
            1.0f
        );

        // 警告パネルっぽい板（Spikeを薄く広げて貼る）
        DrawModel_(
            mdlSpike_,
            {b.x + 0.10f, b.y + 0.26f, 21.7f},
            {W * 0.22f, H * 0.10f, 0.05f},
            {0,0, rotZDeg - 12.0f},
            1.0f
        );
        DrawModel_(
            mdlSpike_,
            {b.x + 0.08f, b.y + 0.28f, 21.6f},
            {W * 0.24f, H * 0.11f, 0.05f},
            {0,0, rotZDeg - 14.0f},
            1.0f
        );
        };

    Flood({-W * 0.48f, H * 0.82f, 0}, 10.0f);
    Flood({W * 0.52f, H * 0.74f, 0}, 18.0f);
}
