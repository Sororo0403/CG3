#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <cassert>

/// <summary>
/// D3D12 のバッファ作成やビュー生成を補助するユーティリティ関数群。
/// </summary>
namespace BufferUtil {

    /// <summary>
    /// 定数バッファを 256B アラインに揃える。<br/>
    /// CBV は必ず 256 バイト単位に揃える必要がある。
    /// </summary>
    /// <param name="size">バイト数。</param>
    /// <returns>アライン済みのバイト数。</returns>
    inline UINT AlignConstantBufferSize(UINT size) {
        return (size + 255u) & ~255u;
    }

    /// <summary>
    /// Upload ヒープに配置されたバッファを作成する。<br/>
    /// CPU→GPU 転送用。マップして書き込み可能。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    /// <param name="sizeInBytes">バッファサイズ。</param>
    /// <returns>生成されたリソース。</returns>
    inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device *device, size_t sizeInBytes) {
        assert(device);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeInBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> res;
        HRESULT hr = device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, // Upload は固定
            nullptr,
            IID_PPV_ARGS(&res));
        assert(SUCCEEDED(hr));
        return res;
    }

    /// <summary>
    /// Default ヒープに配置されたバッファを作成する。<br/>
    /// VRAM 上に配置され、高速だが CPU から直接アクセスできない。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    /// <param name="sizeInBytes">バッファサイズ。</param>
    /// <param name="initialState">初期リソースステート（既定: COMMON）。</param>
    /// <returns>生成されたリソース。</returns>
    inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device *device, size_t sizeInBytes, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON) {
        assert(device);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeInBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> res;
        HRESULT hr = device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&res));
        assert(SUCCEEDED(hr));
        return res;
    }

    /// <summary>
    /// Readback ヒープに配置されたバッファを作成する。<br/>
    /// GPU→CPU 転送用。通常 COPY_DEST として使う。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    /// <param name="sizeInBytes">バッファサイズ。</param>
    /// <returns>生成されたリソース。</returns>
    inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device *device, size_t sizeInBytes) {
        assert(device);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeInBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> res;
        HRESULT hr = device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, // Readback は通常 COPY_DEST
            nullptr,
            IID_PPV_ARGS(&res));
        assert(SUCCEEDED(hr));
        return res;
    }

    /// <summary>
    /// 頂点バッファビュー (VBV) を作成する。
    /// </summary>
    /// <param name="buffer">頂点バッファ。</param>
    /// <param name="strideBytes">1頂点あたりのサイズ。</param>
    /// <param name="totalBytes">バッファ全体のサイズ。</param>
    /// <returns>VBV 構造体。</returns>
    inline D3D12_VERTEX_BUFFER_VIEW MakeVBV(ID3D12Resource *buffer, UINT strideBytes, UINT totalBytes) {
        D3D12_VERTEX_BUFFER_VIEW vbv{};
        vbv.BufferLocation = buffer->GetGPUVirtualAddress();
        vbv.StrideInBytes = strideBytes;
        vbv.SizeInBytes = totalBytes;
        return vbv;
    }

    /// <summary>
    /// インデックスバッファビュー (IBV) を作成する。
    /// </summary>
    /// <param name="buffer">インデックスバッファ。</param>
    /// <param name="totalBytes">バッファ全体のサイズ。</param>
    /// <param name="format">インデックスのフォーマット（既定: R32_UINT）。</param>
    /// <returns>IBV 構造体。</returns>
    inline D3D12_INDEX_BUFFER_VIEW MakeIBV(ID3D12Resource *buffer, UINT totalBytes, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) {
        D3D12_INDEX_BUFFER_VIEW ibv{};
        ibv.BufferLocation = buffer->GetGPUVirtualAddress();
        ibv.SizeInBytes = totalBytes;
        ibv.Format = format;
        return ibv;
    }

} // namespace BufferUtil
