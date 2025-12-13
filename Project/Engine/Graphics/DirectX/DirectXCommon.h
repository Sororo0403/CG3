#pragma once

#include <cstdint>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class DirectXCommon {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="hwnd">ウィンドウハンドル</param>
    /// <param name="width">クライアント領域の横幅</param>
    /// <param name="height">クライアント領域の縦幅</param>
    DirectXCommon(HWND hwnd, int width, int height);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~DirectXCommon();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize();

    /// <summary>
    /// 描画前処理
    /// </summary>
    /// <param name="clearColor">描画時のクリアカラー</param>
    void PreDraw(const float clearColor[4]);

    /// <summary>
    /// 描画後処理
    /// </summary>
    void PostDraw();

    /// <summary>
    /// GPU が現在実行中のコマンドをすべて完了するまで待機
    /// </summary>
    void WaitForGpu();

    // Getter
    ID3D12Device *GetDevice() const {
        return device_.Get();
    }
    ID3D12CommandQueue *GetCommandQueue() const {
        return commandQueue_.Get();
    }
    ID3D12GraphicsCommandList *GetCommandList() const {
        return commandList_.Get();
    }
    ID3D12DescriptorHeap *GetSrvHeap() const {
        return srvHeap_.Get();
    }
    UINT GetSrvDescriptorSize() const {
        return descriptorSizeSRV_;
    }
    UINT GetBackBufferIndex() const {
        return currentBackBufferIndex_;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTV() const {
        return rtvHandles_[currentBackBufferIndex_];
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const {
        return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    }
    const D3D12_VIEWPORT &GetViewport() const {
        return viewport_;
    }
    const D3D12_RECT &GetScissorRect() const {
        return scissorRect_;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap,
                                             UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap,
                                             UINT index) const;

  private:
    // Initialize
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeDescriptorHeaps();
    void InitializeBackBuffers();
    void InitializeDepthBuffer();
    void InitializeRenderTargetViews();
    void InitializeDepthStencilView();
    void InitializeFence();
    void InitializeViewport();
    void InitializeScissorRect();
    void InitializeImGui();

    /// <summary>
    /// 指定したバックバッファのフェンス値が完了するまで待機
    /// </summary>
    void WaitForFrame(UINT frameIndex);

    /// <summary>
    /// 指定した種類・サイズのディスクリプタヒープを生成
    /// </summary>
    ID3D12DescriptorHeap *CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                               UINT numDescriptors,
                                               bool shaderVisible);

  private:
    HWND hwnd_;
    int width_;
    int height_;

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[2];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    static constexpr UINT kBufferCount = 2;
    Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers_[kBufferCount];

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil_;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[kBufferCount]{};
    UINT descriptorSizeRTV_ = 0;
    UINT descriptorSizeDSV_ = 0;
    UINT descriptorSizeSRV_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    uint64_t fenceValues_[kBufferCount]{};
    uint64_t nextFenceValue_ = 0;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    UINT currentBackBufferIndex_ = 0;
};
