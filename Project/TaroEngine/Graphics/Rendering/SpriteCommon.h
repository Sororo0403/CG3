#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>

class ShaderCompiler; // 前方宣言

class SpriteCommon {
public:
    struct PipelineFormats {
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
        UINT        numRenderTargets = 1;
    };

public:
    SpriteCommon() = default;
    ~SpriteCommon() = default;

    void Initialize(ID3D12Device *device);

    void CreateGraphicsPipeline(
        ShaderCompiler &compiler,
        ID3D12Device *device,
        const std::wstring &vsPath,
        const std::wstring &psPath,
        const PipelineFormats &formats = {},
        const std::wstring &vsEntry = L"main",
        const std::wstring &psEntry = L"main");

    void ApplyCommonDrawSettings(
        ID3D12GraphicsCommandList *cmd,
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) const;

    ID3D12RootSignature *GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState *GetPipelineState() const { return pipelineState_.Get(); }

private:
    void CreateRootSignature(ID3D12Device *device);

private:
    ID3D12Device *device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
