#pragma once
#include <d3d12.h>
#include <wrl.h>

struct Texture {
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
};