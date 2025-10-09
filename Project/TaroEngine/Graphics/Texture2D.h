#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D() = default;

    // 画像を読み込み、Upload→Copy→Barrier→SRV 作成まで実施
    void CreateFromFile(
        ID3D12Device *device,
        ID3D12GraphicsCommandList *cmd,
        const wchar_t *pathW,
        ID3D12DescriptorHeap *srvHeap,
        UINT srvIndex);

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv() const { return gpuSrv_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrv() const { return cpuSrv_; }
    ID3D12Resource *GetResource() const { return tex_.Get(); }

    uint32_t Width()  const { return width_; }
    uint32_t Height() const { return height_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> tex_;     // Default (VRAM)
    Microsoft::WRL::ComPtr<ID3D12Resource> upload_;  // Upload (作成時一時利用)
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv_{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv_{};
    uint32_t width_ = 0, height_ = 0;
};
