#pragma once
#define NOMINMAX

#include "IScene.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "SceneManager.h"
#include "Camera.h"
#include "Model.h"
#include "Transform.h"
#include "Difficulty.h"

#include <string>
#include <array>
#include <wrl.h>
#include <d3d12.h>

class ClearScene : public IScene {
public:
    ClearScene(float clearTimeSec,
        int nextStageId,
        int playedStageId,
        Difficulty diff)
        : clearTimeSec_(clearTimeSec)
        , nextStageId_(nextStageId)
        , playedStageId_(playedStageId)
        , difficulty_(diff) {
    }

    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Update(float dt) override;
    void Draw() override;
    void Finalize() override;

private:
    // ===== 参照とシーン管理 =====
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;
    SceneManager *sceneManager_ = nullptr;

    Camera camera_;

    //
    // ===== UI用モデル（タイム表示とかメニューの板） =====
    //
    Model titlePlateModel_;    // "CLEAR TIME" 的な板
    Model nextStageModel_;     // "NEXT STAGE"
    Model stageSelectModel_;   // "STAGE SELECT"

    std::array<Model, 10> digitModel_; // 0..9
    Model spaceModel_;                 // ":" "." " " など記号用

    //
    // ===== タイトル背景と同じ環境を描くためのモデル =====
    //
    Model mdlSolid_;
    Model mdlFragileAny_;
    Model mdlFragileTop_;
    Model mdlFragileBottom_;
    Model mdlRegen_;
    Model mdlSpring_;
    Model mdlSpike_;
    Model mdlSwitchOn_;
    Model mdlSwitchOff_;
    Model mdlSwitchBlockOn_;
    Model mdlSwitchBlockOff_;
    Model mdlJumpOnly_;

    //
    // ===== それぞれのアルベドテクスチャを保持するSRV用リソース =====
    //
    Microsoft::WRL::ComPtr<ID3D12Resource> texSolid_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileAny_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileTop_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileBottom_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texRegen_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpring_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpike_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOff_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchBlockOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchBlockOff_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texJumpOnly_;

    //
    // ===== クリア情報 =====
    //
    float     clearTimeSec_ = 0.0f;
    int       nextStageId_ = 0;
    int       playedStageId_ = 1;
    Difficulty difficulty_ = Difficulty::Normal;

    // メニューどっち選んでるか
    // 0 = NEXT STAGE, 1 = STAGE SELECT
    int menuIndex_ = 0;

    //
    // ===== 内部処理関数 =====
    //

    // SRVを作ってModelに割り当てる（TitleScene版と同じことをClearScene側でも）
    bool LoadTextureSRV_(
        const std::wstring &fileU16,
        UINT localSrvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle
    );

    // "mm:ss.mmm" 文字列を組む
    std::string BuildTimeString_() const;

    // 時刻の 1文字ずつを板モデルで描画
    void DrawTimeString3D_(
        ID3D12GraphicsCommandList *cmd,
        const std::string &timeText,
        float baseX, float baseY, float z,
        float charW, float charH,
        float spacing
    );

    // タイトル画面と同じ夜景背景を描画
    void DrawTitleBackground_();
};
