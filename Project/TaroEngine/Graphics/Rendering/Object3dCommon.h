#pragma once
#include "DirectXCommon.h"

/// <summary>
/// 3Dオブジェクトの共通描画を管理するクラス。
/// （ルートシグネチャ・グラフィックスパイプラインなど）
/// </summary>
class Object3dCommon {
private:
    // --- DirectX共通 ---
    DirectXCommon *dxCommon_ = nullptr;

    // --- 各種D3Dリソース ---
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

private:
    /// <summary>
    /// ルートシグネチャを作成
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// グラフィックスパイプラインを作成
    /// </summary>
    void CreateGraphicsPipelineState();

public:
    /// <summary>
    /// 初期化処理
    /// </summary>
    /// <param name="dxCommon">DirectX共通クラス</param>
    void Initialize(DirectXCommon *dxCommon);

    /// <summary>
    /// getter
    /// </summary>
    DirectXCommon *GetDxCommon() const { return dxCommon_; }

    /// <summary>
    /// 共通描画設定（ルール設定）
    /// </summary>
    /// <param name="cmdList">描画コマンドリスト</param>
    void SetCommonDrawSetting(ID3D12GraphicsCommandList *cmdList);
};
