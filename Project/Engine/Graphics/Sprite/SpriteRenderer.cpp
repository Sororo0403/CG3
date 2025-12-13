#include "SpriteRenderer.h"

#include "DirectX/DirectXCommon.h"
#include "Shader/ShaderCompiler.h"
#include "Texture/TextureManager.h"
#include "Sprite.h"
#include "Logger/Logger.h"
#include "DirectX/DirectXUtil.h"

#include <directx/d3dx12.h>
#include <cassert>
#include <cstring>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

SpriteRenderer::SpriteRenderer(DirectXCommon *dx,
                               ShaderCompiler *shaderCompiler,
                               TextureManager *textureManager)
    : dx_(dx), shaderCompiler_(shaderCompiler),
      textureManager_(textureManager) {
}

void SpriteRenderer::Initialize() {
    CreateRootSignature();
    CreatePipelineState();
    CreateGeometry();
    CreateConstantBuffer();
}

void SpriteRenderer::Begin() {
    auto *cmd = dx_->GetCommandList();

    cmd->SetPipelineState(pipelineState_.Get());
    cmd->SetGraphicsRootSignature(rootSignature_.Get());

    ID3D12DescriptorHeap *heaps[] = {dx_->GetSrvHeap()};
    cmd->SetDescriptorHeaps(1, heaps);

    cmd->IASetVertexBuffers(0, 1, &vbView_);
    cmd->IASetIndexBuffer(&ibView_);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // CBリングの書き込み位置をリセット
    cbCursor_ = 0;
}

void SpriteRenderer::Draw(const Sprite &sprite) {
    auto *cmd = dx_->GetCommandList();

    const auto &t = sprite.transform;

    XMMATRIX Tpivot = XMMatrixTranslation(-t.pivot.x, -t.pivot.y, 0.0f);
    XMMATRIX S = XMMatrixScaling(sprite.size.x * t.scale.x,
                                 sprite.size.y * t.scale.y, 1.0f);
    XMMATRIX R = XMMatrixRotationZ(t.rotation);
    XMMATRIX T = XMMatrixTranslation(t.position.x, t.position.y, 0.0f);

    XMMATRIX world = Tpivot * S * R * T;
    XMMATRIX mvp = world * projection_;

    if (cbCursor_ + cbStride_ > cbCapacity_) {
        LOG_ERROR("SpriteRenderer: CB ring overflow. Increase max sprites.");
        return;
    }

    SpriteCB cb{};
    XMStoreFloat4x4(&cb.mvp, XMMatrixTranspose(mvp));

    cb.color = XMFLOAT4(sprite.color.x, sprite.color.y, sprite.color.z,
                        sprite.color.w);
    cb.uvRect = XMFLOAT4(sprite.uvRect.x, sprite.uvRect.y, sprite.uvRect.z,
                         sprite.uvRect.w);

    std::memcpy(mappedCB_ + cbCursor_, &cb, sizeof(cb));

    D3D12_GPU_VIRTUAL_ADDRESS cbGpu =
        constantBuffer_->GetGPUVirtualAddress() + cbCursor_;
    cbCursor_ += cbStride_;

    const auto &tex = textureManager_->GetTexture(sprite.textureId);

    cmd->SetGraphicsRootConstantBufferView(0, cbGpu);
    cmd->SetGraphicsRootDescriptorTable(1, tex.gpuHandle);
    cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void SpriteRenderer::CreateRootSignature() {
    LOG_INFO("SpriteRenderer: CreateRootSignature");

    ID3D12Device *device = dx_->GetDevice();

    D3D12_ROOT_PARAMETER params[2]{};

    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &range;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        LOG_ERROR("SpriteRenderer: RootSignature serialize failed");
        if (error)
            OutputDebugStringA((char *) error->GetBufferPointer());
        assert(false);
    }

    hr = device->CreateRootSignature(0, blob->GetBufferPointer(),
                                     blob->GetBufferSize(),
                                     IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr)) {
        LOG_ERROR("SpriteRenderer: CreateRootSignature failed");
        assert(false);
    }
}

void SpriteRenderer::CreatePipelineState() {
    LOG_INFO("SpriteRenderer: CreatePipelineState");

    ID3D12Device *device = dx_->GetDevice();

    auto vs = shaderCompiler_->CompileShader("Sprite/Sprite.VS.hlsl", "main",
                                             "vs_6_0");
    auto ps = shaderCompiler_->CompileShader("Sprite/Sprite.PS.hlsl", "main",
                                             "ps_6_0");

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature_.Get();
    pso.InputLayout = {layout, _countof(layout)};
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;

    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    auto &rt = pso.BlendState.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &pso, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr)) {
        LOG_ERROR("SpriteRenderer: CreateGraphicsPipelineState failed hr=" +
                  std::to_string((uint32_t) hr));
        assert(false);
    }
}

void SpriteRenderer::CreateGeometry() {
    LOG_INFO("SpriteRenderer: CreateGeometry");

    ID3D12Device *device = dx_->GetDevice();

    struct Vertex {
        float position[3];
        float uv[2];
    };

    Vertex vertices[] = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    };

    uint32_t indices[] = {0, 1, 2, 1, 3, 2};

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    auto vdesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &vdesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&vertexBuffer_));
    if (FAILED(hr)) {
        assert(false);
    }

    void *mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);

    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(vertices);
    vbView_.StrideInBytes = sizeof(Vertex);

    auto idesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices));
    hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &idesc,
                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                         nullptr, IID_PPV_ARGS(&indexBuffer_));
    if (FAILED(hr)) {
        assert(false);
    }

    indexBuffer_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, indices, sizeof(indices));
    indexBuffer_->Unmap(0, nullptr);

    ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(indices);
    ibView_.Format = DXGI_FORMAT_R32_UINT;
}

void SpriteRenderer::CreateConstantBuffer() {
    LOG_INFO("SpriteRenderer: CreateConstantBuffer");

    ID3D12Device *device = dx_->GetDevice();

    cbStride_ = DirectXUtil::Align256((uint32_t) sizeof(SpriteCB));
    cbCapacity_ = cbStride_ * kMaxSpritesPerFrame;

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(cbCapacity_);

    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&constantBuffer_));
    if (FAILED(hr)) {
        LOG_ERROR("SpriteRenderer: CreateConstantBuffer failed");
        assert(false);
    }

    void *p = nullptr;
    hr = constantBuffer_->Map(0, nullptr, &p);
    if (FAILED(hr) || !p) {
        LOG_ERROR("SpriteRenderer: ConstantBuffer Map failed");
        assert(false);
    }

    mappedCB_ = reinterpret_cast<uint8_t *>(p);
    cbCursor_ = 0;
}
