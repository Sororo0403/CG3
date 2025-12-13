#pragma once

#include <d3d12.h>
#include <wrl.h>

class ShaderCompiler;

/// <summary>
/// 各種 Graphics Pipeline State Object（PSO）と
/// RootSignature を一元管理するクラス。
/// </summary>
class PSOManager {
  public:
    /// <summary>
    /// コンストラクタ。
    /// </summary>
    PSOManager(ID3D12Device *device, ShaderCompiler *shaderCompiler);

    /// <summary>
    /// PSOManager を初期化する。
    /// </summary>
    void Initialize();

    // ==============================
    // Sprite
    // ==============================
    ID3D12RootSignature *GetSpriteRoot() const {
        return spriteRoot_.Get();
    }
    ID3D12PipelineState *GetSpritePSO() const {
        return spritePSO_.Get();
    }

  private:
    // 初期化ヘルパー
    void CreateSpritePipeline();

  private:
    ID3D12Device *device_ = nullptr;
    ShaderCompiler *shaderCompiler_ = nullptr;

    // Sprite 用
    Microsoft::WRL::ComPtr<ID3D12RootSignature> spriteRoot_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> spritePSO_;
};
