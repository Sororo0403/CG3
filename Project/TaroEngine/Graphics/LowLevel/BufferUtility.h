#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstddef>

/// <summary>
/// D3D12 のバッファ作成・書き込みユーティリティ。
/// </summary>
namespace BufferUtility {

    using Microsoft::WRL::ComPtr;

    /// <summary>
    /// 定数バッファ用に 256B アラインしたサイズを返す。
    /// </summary>
    inline UINT AlignCB(UINT size) noexcept {
        constexpr UINT A = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        return (size + (A - 1)) & ~(A - 1);
    }

    /// <summary>
    /// 指定サイズのバッファ ResourceDesc を生成する。
    /// </summary>
    inline D3D12_RESOURCE_DESC MakeBufferDesc(uint64_t size) noexcept {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc = {1, 0};
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        return desc;
    }

    /// <summary>
    /// 任意ヒープで CommittedBuffer を生成する。
    /// </summary>
    inline ComPtr<ID3D12Resource> CreateCommittedBuffer(
        ID3D12Device *device,
        const D3D12_HEAP_PROPERTIES &heapProps,
        const D3D12_RESOURCE_DESC &desc,
        D3D12_RESOURCE_STATES initState,
        const D3D12_CLEAR_VALUE *clearValue = nullptr) noexcept {
        assert(device);
        ComPtr<ID3D12Resource> res;
        const HRESULT hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            initState, clearValue, IID_PPV_ARGS(&res));
        assert(SUCCEEDED(hr));
        return res;
    }

    /// <summary>
    /// Upload バッファを生成（CPU 書き込み可）。
    /// </summary>
    inline ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device *device, uint64_t size) noexcept {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        return CreateCommittedBuffer(device, heap, MakeBufferDesc(size), D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    /// <summary>
    /// Default バッファを生成（GPU 専用領域、コピー先）。
    /// </summary>
    inline ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device *device, uint64_t size) noexcept {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        return CreateCommittedBuffer(device, heap, MakeBufferDesc(size), D3D12_RESOURCE_STATE_COPY_DEST);
    }

    /// <summary>
    /// Readback バッファを生成（GPU→CPU 転送読み取り用）。
    /// </summary>
    inline ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device *device, uint64_t size) noexcept {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        return CreateCommittedBuffer(device, heap, MakeBufferDesc(size), D3D12_RESOURCE_STATE_COPY_DEST);
    }

    /// <summary>
    /// Upload バッファへ即時書き込み（Map→memcpy→Unmap）。
    /// </summary>
    inline void WriteToUpload(ID3D12Resource *upload, const void *src, size_t bytes, size_t offset = 0) noexcept {
        assert(upload && src);
        void *p = nullptr;
        D3D12_RANGE range{0, 0}; // 書き込みのみなので空範囲
        const HRESULT hr = upload->Map(0, &range, &p);
        assert(SUCCEEDED(hr));
        std::memcpy(static_cast<std::byte *>(p) + offset, src, bytes);
        upload->Unmap(0, nullptr);
    }

} // namespace BufferUtility
