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
    /// 定数バッファ（Constant Buffer）用のサイズを 256 バイト境界に切り上げて返します。
    /// </summary>
    /// <param name="size">元のサイズ（バイト）。</param>
    /// <returns>256 バイト境界にアラインされたサイズ。</returns>
    inline UINT AlignCB(UINT size) noexcept {
        constexpr UINT A = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256
        return (size + (A - 1)) & ~(A - 1);
    }

    /// <summary>
    /// 指定サイズのバッファ用 <see cref="D3D12_RESOURCE_DESC"/> を生成します。
    /// </summary>
    /// <param name="size">バッファのサイズ（バイト）。</param>
    /// <returns>バッファ用に初期化された <see cref="D3D12_RESOURCE_DESC"/>。</returns>
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
    /// 任意のヒーププロパティで Committed なバッファリソースを生成します。
    /// </summary>
    /// <param name="device">D3D12 デバイス。nullptr ではいけません。</param>
    /// <param name="heapProps">ヒーププロパティ（UPLOAD/DEFAULT/READBACK など）。</param>
    /// <param name="desc">リソース記述子（<see cref="MakeBufferDesc"/> 等で作成）。</param>
    /// <param name="initState">初期サブリソースステート。</param>
    /// <param name="clearValue">クリア値。バッファでは通常不要のため nullptr を指定します（デフォルト）。</param>
    /// <returns>作成された <see cref="ID3D12Resource"/> を保持する <see cref="ComPtr{ID3D12Resource}"/>。</returns>
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
    /// CPU から書き込み可能な Upload バッファを生成します。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    /// <param name="size">バッファサイズ（バイト）。</param>
    /// <returns>Upload ヒープ上に確保されたバッファ。</returns>
    inline ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device *device, uint64_t size) noexcept {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        return CreateCommittedBuffer(device, heap, MakeBufferDesc(size), D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    /// <summary>
    /// GPU 専用領域（DEFAULT ヒープ）のバッファを生成します。初期ステートは COPY_DEST です。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    /// <param name="size">バッファサイズ（バイト）。</param>
    /// <returns>Default ヒープ上に確保されたバッファ。</returns>
    inline ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device *device, uint64_t size) noexcept {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        return CreateCommittedBuffer(device, heap, MakeBufferDesc(size), D3D12_RESOURCE_STATE_COPY_DEST);
    }

    /// <summary>
    /// GPU から CPU へのデータ読み戻し用 Readback バッファを生成します。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    /// <param name="size">バッファサイズ（バイト）。</param>
    /// <returns>Readback ヒープ上に確保されたバッファ。</returns>
    inline ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device *device, uint64_t size) noexcept {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        return CreateCommittedBuffer(device, heap, MakeBufferDesc(size), D3D12_RESOURCE_STATE_COPY_DEST);
    }

    /// <summary>
    /// Upload バッファに対して即時書き込み（Map → memcpy → Unmap）を行います。
    /// </summary>
    /// <param name="upload">対象の Upload バッファ（<see cref="D3D12_HEAP_TYPE_UPLOAD"/>）。</param>
    /// <param name="src">コピー元メモリへのポインタ。</param>
    /// <param name="bytes">コピーするサイズ（バイト）。</param>
    /// <param name="offset">Upload バッファ内オフセット（バイト）。既定は 0。</param>
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
