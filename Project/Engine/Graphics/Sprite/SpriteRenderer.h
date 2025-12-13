#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include <cstdint>

struct Sprite;
class DirectXCommon;
class TextureManager;
class ShaderCompiler;

/// <summary>
/// 2D スプライト描画専用レンダラー。
/// Sprite のデータを受け取り、DirectX12 で描画する。
/// </summary>
class SpriteRenderer {
  public:
    /// <summary>
    /// SpriteRenderer を生成する。
    /// 描画に必要な DirectX / Shader / Texture 管理を受け取る。
    /// </summary>
    /// <param name="dx">DirectX 共通管理クラス</param>
    /// <param name="shaderCompiler">シェーダコンパイラ</param>
    /// <param name="textureManager">テクスチャ管理クラス</param>
    SpriteRenderer(DirectXCommon *dx, ShaderCompiler *shaderCompiler,
                   TextureManager *textureManager);

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize();

    /// <summary>
    /// 描画前準備（PSO / RootSignature / CB リング初期化）
    /// </summary>
    void Begin();

    /// <summary>
    /// 指定された Sprite を1枚描画する。
    /// </summary>
    void Draw(const Sprite &sprite);

    /// <summary>
    /// スプライト用の射影行列を設定する。
    /// </summary>
    void SetProjection(const DirectX::XMMATRIX &proj) {
        projection_ = proj;
    }

  private:
    struct SpriteCB {
        DirectX::XMFLOAT4X4 mvp;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 uvRect;
    };

  private:
    // Create
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateGeometry();
    void CreateConstantBuffer();

  private:
    static constexpr uint32_t kMaxSpritesPerFrame = 1024;

  private:
    DirectXCommon *dx_ = nullptr;
    ShaderCompiler *shaderCompiler_ = nullptr;
    TextureManager *textureManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    uint8_t *mappedCB_ = nullptr;
    uint32_t cbStride_ = 0;
    uint32_t cbCursor_ = 0;
    uint32_t cbCapacity_ = 0;

    DirectX::XMMATRIX projection_ = DirectX::XMMatrixIdentity();
};
