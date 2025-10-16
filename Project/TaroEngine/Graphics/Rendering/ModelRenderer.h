#pragma once

#include <d3d12.h>
#include <wrl.h>

class Mesh;
class ShaderCompiler;

class ModelRenderer {
public:
    void Initialize(ID3D12Device *device,
        ShaderCompiler *shader);   // ★ 変更：ShaderCompiler を受け取る

    void Finalize() noexcept;

    void Begin(ID3D12GraphicsCommandList *cmd,
        const float view[16], const float proj[16]) noexcept;

    void End(ID3D12GraphicsCommandList *cmd) noexcept;

    void Draw(ID3D12GraphicsCommandList *cmd,
        const Mesh &mesh,
        const float world[16],
        const float color[4]) const noexcept;

private:
    void CreateRootSignature();
    void CreatePipelineState();

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

    struct SceneCB { float view[16]; float proj[16]; };
    struct ObjectCB { float world[16]; float color[4]; };

    Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> objectCB_;
    SceneCB *sceneCBMapped_ = nullptr;
    ObjectCB *objectCBMapped_ = nullptr;

    ShaderCompiler *shader_ = nullptr;   // ★ 追加
};
