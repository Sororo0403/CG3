#include "SpriteRenderer.h"

#include "DirectX/DirectXCommon.h"
#include "Shader/ShaderCompiler.h"
#include "Texture/TextureManager.h"
#include "PSO/PSOManager.h"
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
                               TextureManager *textureManager,
                               PSOManager *psoManager, float width,
                               float height)
    : dx_(dx), shaderCompiler_(shaderCompiler), textureManager_(textureManager),
      psoManager_(psoManager) {
    projection_ =
        XMMatrixOrthographicOffCenterLH(0.0f, width, height, 0.0f, 0.0f, 1.0f);
}

void SpriteRenderer::Initialize() {
    CreateGeometry();
    CreateConstantBuffer();
}

void SpriteRenderer::Begin() {
    auto *cmd = dx_->GetCommandList();

    cmd->SetPipelineState(psoManager_->GetSpritePSO());
    cmd->SetGraphicsRootSignature(psoManager_->GetSpriteRoot());

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
