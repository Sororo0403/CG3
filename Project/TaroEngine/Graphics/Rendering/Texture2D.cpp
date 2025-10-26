#define NOMINMAX
#include "Texture2D.h"

#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"

#include <vector>
#include <filesystem>
#include <cwctype>   // towlower
#include <windows.h> // OutputDebugStringW
#include <cassert>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {
    DXGI_FORMAT ToSRGBIfNeeded(DXGI_FORMAT fmt, bool forceSRGB) {
        if (!forceSRGB) return fmt;
        switch (fmt) {
        case DXGI_FORMAT_R8G8B8A8_UNORM:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8A8_UNORM:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8X8_UNORM:   return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        default: return fmt;
        }
    }

    inline void WaitForQueueIdle(
        ID3D12CommandQueue *queue,
        ID3D12Fence *fence,
        HANDLE fenceEvent,
        UINT64 &nextFenceValue) {
        const UINT64 fv = ++nextFenceValue;
        queue->Signal(fence, fv);
        if (fence->GetCompletedValue() < fv) {
            fence->SetEventOnCompletion(fv, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }
}

void Texture2D::Reset() noexcept {
    upload_.Reset();
    tex_.Reset();
    gpuSrv_ = {};
    srvIndex_ = UINT(-1);
}

bool Texture2D::InitializeFromFile(
    ID3D12Device *device,
    ID3D12CommandQueue *commandQueue,
    ID3D12Fence *fence,
    HANDLE fenceEvent,
    UINT64 &nextFenceValue,
    ID3D12DescriptorHeap *srvHeap,
    UINT srvIndex,
    const std::wstring &filepath,
    D3D12_GPU_DESCRIPTOR_HANDLE fallbackSrvGpu,
    bool forceSRGB) {
    assert(device);
    assert(commandQueue);
    assert(fence);
    assert(fenceEvent);
    assert(srvHeap);

    Reset();

    // 1) 画像読込 (DDS or WIC)
    ScratchImage img;
    TexMetadata meta{};
    HRESULT hr = E_FAIL;

    std::wstring ext = std::filesystem::path(filepath).extension().wstring();
    for (auto &c : ext) { c = (wchar_t)::towlower(c); }

    if (ext == L".dds") {
        hr = LoadFromDDSFile(filepath.c_str(), DDS_FLAGS_NONE, &meta, img);
    } else {
        hr = LoadFromWICFile(filepath.c_str(), WIC_FLAGS_FORCE_RGB, &meta, img);
    }

    if (FAILED(hr)) {
        // 失敗 → フォールバック登録だけ
        OutputDebugStringW((L"[Texture2D] Failed load: " + filepath + L"\n").c_str());
        gpuSrv_ = fallbackSrvGpu;
        srvIndex_ = srvIndex;
        return false;
    }

    meta.format = ToSRGBIfNeeded(meta.format, forceSRGB);

    // 2) DefaultHeap テクスチャ
    CD3DX12_HEAP_PROPERTIES heapDefault(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC   texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        meta.format,
        static_cast<UINT>(meta.width),
        static_cast<UINT>(meta.height),
        static_cast<UINT16>(meta.arraySize),
        static_cast<UINT16>(meta.mipLevels));

    hr = device->CreateCommittedResource(
        &heapDefault,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&tex_));
    if (FAILED(hr) || !tex_) {
        OutputDebugStringW(L"[Texture2D] CreateCommittedResource(default) failed\n");
        gpuSrv_ = fallbackSrvGpu;
        srvIndex_ = srvIndex;
        return false;
    }

    // 3) アップロードバッファ
    std::vector<D3D12_SUBRESOURCE_DATA> subs;
    subs.reserve(img.GetImageCount());

    const Image *images = img.GetImages();
    for (size_t i = 0; i < img.GetImageCount(); ++i) {
        D3D12_SUBRESOURCE_DATA s{};
        s.pData = images[i].pixels;
        s.RowPitch = images[i].rowPitch;
        s.SlicePitch = images[i].slicePitch;
        subs.push_back(s);
    }

    const UINT64 uploadBytes =
        GetRequiredIntermediateSize(tex_.Get(), 0, static_cast<UINT>(subs.size()));

    CD3DX12_HEAP_PROPERTIES heapUpload(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBytes);

    hr = device->CreateCommittedResource(
        &heapUpload,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&upload_));
    if (FAILED(hr) || !upload_) {
        OutputDebugStringW(L"[Texture2D] CreateCommittedResource(upload) failed\n");
        gpuSrv_ = fallbackSrvGpu;
        srvIndex_ = srvIndex;
        return false;
    }

    // 4) 一時コマンドでコピー＆バリア
    ComPtr<ID3D12CommandAllocator> ca;
    ComPtr<ID3D12GraphicsCommandList> cl;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ca));
    if (FAILED(hr) || !ca) {
        OutputDebugStringW(L"[Texture2D] CreateCommandAllocator failed\n");
        gpuSrv_ = fallbackSrvGpu;
        srvIndex_ = srvIndex;
        return false;
    }
    hr = device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, ca.Get(), nullptr, IID_PPV_ARGS(&cl));
    if (FAILED(hr) || !cl) {
        OutputDebugStringW(L"[Texture2D] CreateCommandList failed\n");
        gpuSrv_ = fallbackSrvGpu;
        srvIndex_ = srvIndex;
        return false;
    }

    UpdateSubresources(
        cl.Get(),
        tex_.Get(),
        upload_.Get(),
        0, 0,
        static_cast<UINT>(subs.size()), subs.data());

    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            tex_.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
    cl->ResourceBarrier(1, &barrier);

    cl->Close();

    ID3D12CommandList *lists[] = {cl.Get()};
    commandQueue->ExecuteCommandLists(1, lists);

    // 5) GPU待機
    WaitForQueueIdle(commandQueue, fence, fenceEvent, nextFenceValue);

    // 6) SRVを srvHeap[srvIndex] に作成
    const UINT inc =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(srvIndex) * inc;

    D3D12_GPU_DESCRIPTOR_HANDLE gpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += static_cast<UINT64>(srvIndex) * inc;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = meta.format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(meta.mipLevels);
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    device->CreateShaderResourceView(tex_.Get(), &srvDesc, cpu);

    // 記録
    gpuSrv_ = gpu;
    srvIndex_ = srvIndex;

    return true;
}
