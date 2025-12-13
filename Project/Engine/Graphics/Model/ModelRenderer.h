#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "Camera/Camera.h"

class DirectXCommon;
class PSOManager;
class MeshManager;
struct Model;

class ModelRenderer {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    ModelRenderer(DirectXCommon *dx, PSOManager *psoManager,
                  MeshManager *meshManager);

    /// <summary>
    /// GPU リソース生成
    /// </summary>
    void Initialize();

    /// <summary>
    /// フレーム開始
    /// </summary>
    void Begin();

    /// <summary>
    /// Model を1つ描画
    /// </summary>
    void Draw(const Model &model, const Camera *camera);

  private:
    // Create
    void CreateConstantBuffer();

  private:
    static constexpr uint32_t kMaxModelsPerFrame = 1024;

  private:
    DirectXCommon *dx_ = nullptr;
    PSOManager *psoManager_ = nullptr;
    MeshManager *meshManager_ = nullptr;

    // Constant Buffer Ring
    struct ModelCB {
        DirectX::XMFLOAT4X4 wvp;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    uint8_t *mappedCB_ = nullptr;

    uint32_t cbStride_ = 0;
    uint32_t cbCursor_ = 0;
    uint32_t cbCapacity_ = 0;
};
