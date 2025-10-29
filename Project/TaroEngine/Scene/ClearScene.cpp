#define NOMINMAX
#include "ClearScene.h"

#include "Input.h"
#include "ModelRenderer.h"
#include "GameScene.h"
#include "StageSelectScene.h"
#include "DirectXCommon.h"
#include "BufferUtility.h"

#include <format>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <Windows.h> // for MultiByteToWideChar
#include <DirectXTex/DirectXTex.h>
#include <DirectXTex/d3dx12.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static inline float Deg(float d) {
    return XMConvertToRadians(d);
}

// === UTF-8→UTF-16 (TitleScene側のものと同じロジック) ===
static std::wstring WidenU16_Local_(const std::string &s) {
    if (s.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

namespace {
    // ClearScene用のSRV割り当て開始オフセット
    // TitleSceneと被らなければOK。被っても同時に表示しないなら実害は薄いけど、
    // わかりやすくズラしておく。
    constexpr UINT kClearSrvBase = 48;
}

// =======================================================
// 共通の板描画ヘルパ (UIのメニューや数字用)
// =======================================================
static void DrawPlate(
    ModelRenderer *renderer,
    ID3D12GraphicsCommandList *cmd,
    Model &m,
    const XMFLOAT3 &pos,
    const XMFLOAT3 &scale,
    float rotZDeg,
    float alphaMul = 1.0f
) {
    Transform t{};
    t.pos = pos;
    t.scale = scale;
    t.rot = {0.0f, 0.0f, Deg(rotZDeg)};

    renderer->Draw(cmd, m, t, alphaMul);
}

// =======================================================
// Initialize
// =======================================================
void ClearScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engineContext_ = engine;
    renderContext_ = render;
    sceneManager_ = engineContext_->sceneManager;

    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    //
    // ===== カメラをUI用に設定 =====
    //
    {
        float w = (float)dx->GetWidth();
        float h = (float)dx->GetHeight();
        float aspect = w / std::max(1.0f, h);

        // カメラ自体は -10 の位置から原点を見る
        camera_.Initialize(
            {0.0f, 0.0f, -10.0f}, // eye
            {0.0f, 0.0f,  0.0f},  // target
            60.0f,
            aspect,
            0.1f,
            1000.0f
        );

        // 正射影行列
        // X方向: [-aspect, +aspect]
        // Y方向: [-1, +1]
        float orthoW = aspect * 2.0f;
        float orthoH = 2.0f;
        camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);

        camera_.LookAt(
            {0.0f, 0.0f, -10.0f},
            {0.0f, 0.0f,   0.0f}
        );

        camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    }

    //
    // ===== モデル読み込み (UI文字とか) =====
    //
    // NOTE: プロジェクトで用意してる.objに合わせてパスは調整すること
    titlePlateModel_.Initialize(device, "Resources/Model/normal.obj");         // CLEAR TIME 看板用
    nextStageModel_.Initialize(device, "Resources/Model/nextStage.obj");      // NEXT STAGE
    stageSelectModel_.Initialize(device, "Resources/Model/stageSelect.obj");    // STAGE SELECT

    digitModel_[0].Initialize(device, "Resources/Model/0.obj");
    digitModel_[1].Initialize(device, "Resources/Model/1.obj");
    digitModel_[2].Initialize(device, "Resources/Model/2.obj");
    digitModel_[3].Initialize(device, "Resources/Model/3.obj");
    digitModel_[4].Initialize(device, "Resources/Model/4.obj");
    digitModel_[5].Initialize(device, "Resources/Model/5.obj");
    digitModel_[6].Initialize(device, "Resources/Model/6.obj");
    digitModel_[7].Initialize(device, "Resources/Model/7.obj");
    digitModel_[8].Initialize(device, "Resources/Model/8.obj");
    digitModel_[9].Initialize(device, "Resources/Model/9.obj");

    spaceModel_.Initialize(device, "Resources/Model/space.obj"); // ":" "." " " とか記号用の板モデル

    //
    // ===== タイトル背景用に使ってるブロック系モデルも読み込む =====
    //
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

    //
    // ===== テクスチャSRVの割り当て =====
    //
    auto setupTex = [&](Model &m, UINT localIndex, ComPtr<ID3D12Resource> &holder) {
        if (m.GetAlbedoPath().empty()) return;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        if (LoadTextureSRV_(WidenU16_Local_(m.GetAlbedoPath()), localIndex, holder, gpu)) {
            m.SetAlbedoSRV(gpu);
        }
        };

    // ClearSceneは ClearSrvBase+localIndex にSRVを作る。
    // localIndex は 0,1,2,... でOK。
    setupTex(mdlSolid_, 0, texSolid_);
    setupTex(mdlFragileAny_, 1, texFragileAny_);
    setupTex(mdlFragileTop_, 2, texFragileTop_);
    setupTex(mdlFragileBottom_, 3, texFragileBottom_);
    setupTex(mdlRegen_, 4, texRegen_);
    setupTex(mdlSpring_, 5, texSpring_);
    setupTex(mdlSpike_, 6, texSpike_);
    setupTex(mdlSwitchOn_, 7, texSwitchOn_);
    setupTex(mdlSwitchOff_, 8, texSwitchOff_);
    setupTex(mdlSwitchBlockOn_, 9, texSwitchBlockOn_);
    setupTex(mdlSwitchBlockOff_, 10, texSwitchBlockOff_);
    setupTex(mdlJumpOnly_, 11, texJumpOnly_);
}

// =======================================================
// Update
// =======================================================
void ClearScene::Update(float /*dt*/) {
    auto *in = engineContext_->input;

    // 上下でメニュー選択
    if (in->IsKeyPressed(DIK_W)) {
        menuIndex_ = 0;
    }
    if (in->IsKeyPressed(DIK_S)) {
        menuIndex_ = 1;
    }

    // SPACEで決定
    if (in->IsKeyPressed(DIK_SPACE)) {
        if (menuIndex_ == 0) {
            // NEXT STAGE
            sceneManager_->ChangeScene(
                std::make_unique<GameScene>(nextStageId_, difficulty_));
        } else {
            // STAGE SELECT に戻る
            sceneManager_->ChangeScene(
                std::make_unique<StageSelectScene>(playedStageId_, difficulty_));
        }
    }
}

// =======================================================
// BuildTimeString_ : mm:ss.mmm を組み立て
// =======================================================
std::string ClearScene::BuildTimeString_() const {
    int totalMs = (int)(clearTimeSec_ * 1000.0f + 0.5f);
    int minutes = totalMs / 60000;
    int msLeft = totalMs % 60000;
    int seconds = msLeft / 1000;
    int millis = msLeft % 1000;

    return std::format("{:02d}:{:02d}.{:03d}",
        minutes, seconds, millis);
}

// =======================================================
// DrawTimeString3D_ : 時間の文字列を1文字ずつ板モデルで描く
// =======================================================
void ClearScene::DrawTimeString3D_(
    ID3D12GraphicsCommandList *cmd,
    const std::string &timeText,
    float baseX, float baseY, float z,
    float charW, float charH,
    float spacing
) {
    auto *renderer = renderContext_->modelRenderer;

    // 全体幅から中央寄せ
    float totalW = (float)timeText.size() * spacing;
    float originX = baseX - totalW * 0.5f;

    for (size_t i = 0; i < timeText.size(); ++i) {
        char c = timeText[i];
        Model *m = nullptr;

        if (c >= '0' && c <= '9') {
            m = &digitModel_[c - '0'];
        } else if (c == ':' || c == '.' || c == ' ') {
            m = &spaceModel_;
        } else {
            continue;
        }

        float x = originX + (float)i * spacing;

        XMFLOAT3 pos = {x, baseY, z};
        XMFLOAT3 scale = {charW, charH, 0.05f};

        DrawPlate(renderer, cmd, *m, pos, scale, 0.0f, 1.0f);
    }
}

// =======================================================
// DrawTitleBackground_ : タイトル画面と同じ夜景・手すりなど
// =======================================================
void ClearScene::DrawTitleBackground_() {
    // ClearScene のカメラ座標系:
    //   X ≈ [-aspect, +aspect]
    //   Y ≈ [-1, +1]
    // つまり「画面全体＝だいたい横2×縦2の正方形っぽい世界」なので
    // そのスケールで優しく塗りつぶすだけにする。
    //
    // TitleSceneみたいなクレーン/街並み/手すりは入れない
    // （スケールが全然違うのでUIをぶった切るから）

    auto &renderer = *renderContext_->modelRenderer;
    auto *cmd = engineContext_->directXCommon->GetCommandList();

    // helper: TitleScene::DrawModel_ 相当の簡略版
    auto DrawQuad = [&](Model &m,
        const XMFLOAT3 &pos,
        const XMFLOAT3 &fullScale,
        float rotZDeg,
        float alphaMul) {
            Transform tr{};
            tr.pos = pos;
            tr.scale = {
                fullScale.x * 0.5f,
                fullScale.y * 0.5f,
                fullScale.z * 0.5f
            };
            tr.rot = {
                0.0f,
                0.0f,
                XMConvertToRadians(rotZDeg)
            };
            renderer.Draw(cmd, m, tr, alphaMul);
        };

    // 画面全体を覆う「空の板」
    // mdlSpring_ は青っぽいテクスチャ想定（TitleSceneで空に使ってたやつ）
    //
    // Zを +5.0f にして UI(0.0fあたり) より奥に置く。
    // scaleは画面いっぱいになるように大きめ（2x2ちょい超えるくらい）
    DrawQuad(
        mdlSpring_,
        /*pos*/      XMFLOAT3{0.0f, 0.0f, 5.0f},
        /*scaleXYZ*/ XMFLOAT3{4.0f, 3.0f, 0.2f},
        /*rotZ*/     0.0f,
        /*alpha*/    1.0f
    );

    // ほんのりグラデーションっぽく重ねる板（薄い別色にしたいやつ）
    // mdlJumpOnly_ をちょい傾けてうっすら上から差し色にするイメージ
    // 透明マルチプライヤは1.0fのままでもOKだけど、もし強かったら0.6fとかに落としていい
    DrawQuad(
        mdlJumpOnly_,
        /*pos*/      XMFLOAT3{0.0f, 0.3f, 4.9f},
        /*scaleXYZ*/ XMFLOAT3{4.0f, 2.5f, 0.2f},
        /*rotZ*/     -4.0f,
        /*alpha*/    1.0f
    );

    // これで背景はふわっと青いグラデだけ。黒い手すり/斜めバーは描かないので消える。
}


// =======================================================
// Draw
// =======================================================
void ClearScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *renderer = renderContext_->modelRenderer;

    renderer->Begin(cmd, dx, camera_);

    //
    // 背景（タイトルと同じ工事現場の夜景）
    //
    DrawTitleBackground_();

    //
    // CLEAR TIME 看板
    //
    {
        XMFLOAT3 pos = {0.0f,  0.60f, 0.0f};
        XMFLOAT3 scale = {0.80f, 0.18f, 0.05f};

        DrawPlate(renderer, cmd, titlePlateModel_, pos, scale, 0.0f, 1.0f);
    }

    //
    // クリアタイム mm:ss.mmm
    //
    {
        std::string t = BuildTimeString_();

        float baseX = 0.0f;
        float baseY = 0.20f;
        float z = 0.0f;
        float charW = 0.10f;
        float charH = 0.18f;
        float spacing = 0.12f;

        DrawTimeString3D_(
            cmd,
            t,
            baseX,
            baseY,
            z,
            charW,
            charH,
            spacing
        );
    }

    //
    // メニュー: NEXT STAGE
    //
    {
        float xOffset = (menuIndex_ == 0) ? -0.5f : -0.2f;

        XMFLOAT3 pos = {xOffset, -0.20f, 0.0f};
        XMFLOAT3 scale = {0.2f,    0.16f,  0.05f};

        DrawPlate(renderer, cmd, nextStageModel_, pos, scale, 0.0f, 1.0f);
    }

    //
    // メニュー: STAGE SELECT
    //
    {
        float xOffset = (menuIndex_ == 1) ? -0.5f : -0.2f;

        XMFLOAT3 pos = {xOffset, -0.45f, 0.0f};
        XMFLOAT3 scale = {0.2f,    0.16f,  0.05f};

        DrawPlate(renderer, cmd, stageSelectModel_, pos, scale, 0.0f, 1.0f);
    }

    renderer->End(cmd);
}

// =======================================================
// Finalize
// =======================================================
void ClearScene::Finalize() {
    // ComPtr が勝手に解放
}

// =======================================================
// LoadTextureSRV_ : ClearScene版
// (TitleScene::LoadTextureSRV_ とほぼ同じだけど
//  kClearSrvBase を使ってSRVの場所をズラすようにしてる)
// =======================================================
bool ClearScene::LoadTextureSRV_(
    const std::wstring &fileU16,
    UINT localSrvIndex,
    ComPtr<ID3D12Resource> &outTex,
    D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle
) {
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

    UINT64 uploadSize =
        GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);

    ComPtr<ID3D12Resource> upload =
        BufferUtility::CreateUploadBuffer(device, uploadSize);

    // 一時コマンドでアップロード
    ComPtr<ID3D12CommandQueue> queue;
    {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    }
    ComPtr<ID3D12CommandAllocator> alloc;
    device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));

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

    // ==== SRVを共通SRVヒープの (kClearSrvBase + localSrvIndex) に作る ====
    ID3D12DescriptorHeap *srvHeap = engineContext_->directXCommon->GetSrvHeap();
    UINT inc = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    UINT srvIndexAbs = kClearSrvBase + localSrvIndex;

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += SIZE_T(inc) * srvIndexAbs;

    D3D12_GPU_DESCRIPTOR_HANDLE gpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(inc) * srvIndexAbs;
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
