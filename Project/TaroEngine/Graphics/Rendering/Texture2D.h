#pragma once
#define NOMINMAX
#include <d3d12.h>
#include <wrl.h>
#include <string>

/// <summary>
/// 単一2Dテクスチャ + そのSRVを管理する簡易ラッパ。
/// InitializeFromFile() は呼び出し時にGPUへ転送＆フェンス待機まで完了する。
/// </summary>
class Texture2D {
public:
    /// <summary>
    /// 画像からテクスチャを作成し、srvHeap[srvIndex] に SRV を作る。
    /// この中で一時コマンドリストを作成・実行・フェンス待機まで行う。
    /// 読み込みに失敗した場合は false を返し、その場合 gpuSrv_ には fallbackSrvGpu を入れる。
    /// </summary>
    /// <param name="device">D3D12デバイス</param>
    /// <param name="commandQueue">DIRECTキュー</param>
    /// <param name="fence">同期用フェンス</param>
    /// <param name="fenceEvent">フェンス待機用イベントハンドル</param>
    /// <param name="nextFenceValue">フェンス用 カウンタ (参照で++される)</param>
    /// <param name="srvHeap">CBV/SRV/UAV 用ディスクリプタヒープ (シェーダ可視)</param>
    /// <param name="srvIndex">ヒープ内に確保済みのインデックス</param>
    /// <param name="filepath">画像ファイルのパス</param>
    /// <param name="fallbackSrvGpu">失敗時に使うフォールバックSRV(GPUハンドル)</param>
    /// <param name="forceSRGB">代表的なUNORMフォーマットをSRGBに変換するか</param>
    /// <returns>成功なら true / 失敗なら false</returns>
    bool InitializeFromFile(
        ID3D12Device *device,
        ID3D12CommandQueue *commandQueue,
        ID3D12Fence *fence,
        HANDLE fenceEvent,
        UINT64 &nextFenceValue,
        ID3D12DescriptorHeap *srvHeap,
        UINT srvIndex,
        const std::wstring &filepath,
        D3D12_GPU_DESCRIPTOR_HANDLE fallbackSrvGpu,
        bool forceSRGB = true
    );

    void Reset() noexcept;

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu()  const noexcept { return gpuSrv_; }
    UINT                        GetSrvIndex()const noexcept { return srvIndex_; }
    bool                        IsValid()    const noexcept { return tex_ != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> tex_;     // GPU側テクスチャ
    Microsoft::WRL::ComPtr<ID3D12Resource> upload_;  // アップロード一時バッファ
    D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv_{};           // シェーダから参照するSRVハンドル
    UINT srvIndex_ = UINT(-1);
};
