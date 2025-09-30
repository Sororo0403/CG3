#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <cassert>

namespace BufferUtil {

	/// <summary>
	/// 256B アライン（CB の必須揃え）
	/// </summary>
	inline UINT AlignConstantBufferSize(UINT size) {
		return (size + 255u) & ~255u;
	}

	/// <summary>
	/// Upload ヒープのコミット済みバッファ（CPU→GPU）を作成します。
	/// </summary>
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
			D3D12_RESOURCE_STATE_GENERIC_READ, // Upload はこれ固定
			nullptr,
			IID_PPV_ARGS(&res));
		assert(SUCCEEDED(hr));
		return res;
	}

	/// <summary>
	/// Default ヒープのコミット済みバッファ（VRAM）。初期状態を指定できます。
	/// </summary>
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
	/// Readback ヒープのコミット済みバッファ（GPU→CPU）を作成します。
	/// </summary>
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
			D3D12_RESOURCE_STATE_COPY_DEST, // Readback は通常 COPY_DEST で受ける
			nullptr,
			IID_PPV_ARGS(&res));
		assert(SUCCEEDED(hr));
		return res;
	}

	/// <summary>
	/// VBV を作成します。
	/// </summary>
	inline D3D12_VERTEX_BUFFER_VIEW MakeVBV(ID3D12Resource *buffer, UINT strideBytes, UINT totalBytes) {
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		vbv.BufferLocation = buffer->GetGPUVirtualAddress();
		vbv.StrideInBytes = strideBytes;
		vbv.SizeInBytes = totalBytes;
		return vbv;
	}

	/// <summary>
	/// IBV を作成します。
	/// </summary>
	inline D3D12_INDEX_BUFFER_VIEW MakeIBV(ID3D12Resource *buffer, UINT totalBytes, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) {
		D3D12_INDEX_BUFFER_VIEW ibv{};
		ibv.BufferLocation = buffer->GetGPUVirtualAddress();
		ibv.SizeInBytes = totalBytes;
		ibv.Format = format;
		return ibv;
	}

} // namespace BufferUtil
