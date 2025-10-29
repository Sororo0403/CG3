#pragma once
#define NOMINMAX

#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <cstdint>

class ShaderCompiler;
class Mesh;
class Model;
class Camera;
class DirectXCommon;
struct Transform;

class ModelRenderer {
public:
    static constexpr uint32_t kFrameCount = 3;
    static constexpr uint32_t kMaxObjectsPerFrame = 4096;

    struct SceneCB {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
    };

    // HLSLのcbuffer(ObjectCB)とメモリレイアウトを完全一致させる
    struct ObjectCB {
        DirectX::XMFLOAT4X4 world;   // gWorld
        DirectX::XMFLOAT4   color;   // gColor (今は未使用)
        float               alphaMul;   // gAlphaMul
        uint32_t            useTexture; // gUseTex
        DirectX::XMFLOAT2   pad;        // _pad_
    };
    static_assert(sizeof(ObjectCB) % 16 == 0, "ObjectCB must be 16-byte aligned");

    void Initialize(ID3D12Device *device, ShaderCompiler *shader);
    void Finalize() noexcept;

    void Begin(
        ID3D12GraphicsCommandList *commandList,
        DirectXCommon *dx,
        const DirectX::XMMATRIX &view,
        const DirectX::XMMATRIX &proj) noexcept;

    void Begin(
        ID3D12GraphicsCommandList *commandList,
        DirectXCommon *dx,
        const Camera &camera) noexcept;

    void End(ID3D12GraphicsCommandList *commandList) noexcept;

    // alphaMulを渡せるDraw。通常は1.0f、半透明にしたいときは<1.0f
    void Draw(
        ID3D12GraphicsCommandList *commandList,
        const Model &model,
        const Transform &transform,
        float alphaMul = 1.0f) noexcept;

private:
    void CreateRootSignature();
    void CreatePipelineState();

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    ShaderCompiler *shader_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

    Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> objectCBs_[kFrameCount];

    uint32_t objectCBStride_ = 0;
    uint32_t objectCBTotalBytes_ = 0;
    uint32_t objectCBOffset_[kFrameCount]{};

    uint32_t currentFrameIndex_ = 0;
};
