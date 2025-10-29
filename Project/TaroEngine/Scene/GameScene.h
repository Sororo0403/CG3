#pragma once
#define NOMINMAX

#include "EngineContext.h"
#include "RenderContext.h"
#include "IScene.h"
#include "DirectXCommon.h"
#include "Camera.h"
#include "Model.h"
#include "Transform.h"
#include "SceneManager.h"

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>
#include <wrl.h>
#include <cmath>
#include <cstring>

struct AABB { float x, y; float w, h; };

/// <summary>
/// 2D横アクション本編シーン（タイル＋物理＋ギミック）
/// 深夜工事現場っぽい足場 / スイッチ / 壊れる床
/// </summary>
class GameScene : public IScene {
public:
    GameScene(int stageId = 1) : stageId_(stageId) {}
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void Finalize() override;

private:
    void GoToClearScene_();
    bool AllFragileGone_() const;

    // ===== 基本定数 =====
    static constexpr int   kMapW = 26;
    static constexpr int   kMapH = 15;
    static constexpr float kTile = 1.0f;

    // 物理パラメータ
    static constexpr float kGravity = 0.55f / 48.0f;
    static constexpr float kMoveGround = 5.0f / 48.0f;
    static constexpr float kMoveAir = 3.0f / 48.0f;
    static constexpr float kJumpVy = 11.0f / 48.0f;
    static constexpr float kSpringVy = 18.0f / 48.0f;
    static constexpr float kMaxFallVy = -18.0f / 48.0f;

    // スキン（めり込み防止のほんの少しの隙間）
    static constexpr float kSkinY = 0.005f;
    static constexpr float kSkinX = 0.005f;

    // 接地・側面判定の最低オーバーラップ
    static constexpr float kMinSideOverlap = 4.0f / 48.0f;
    static constexpr float kMinGroundOverlap = 4.0f / 48.0f;

    // ジャンプ安定化
    static constexpr int kCoyoteMaxFrames = 6;
    static constexpr int kJumpBufferFrames = 6;

    // ===== タイル種別 =====
    enum class Tile : int32_t {
        Empty = 0,
        Solid,
        FragileAny,
        FragileTop,
        FragileBottom,
        Spring,
        Spike,
        JumpOnly,
        Regen,

        // プレイヤーが踏んでON/OFFをトグルするスイッチ本体
        Switch,

        // スイッチ状態に応じて出たり消えたりする足場
        SwitchBlockOn,   // switchOn_ == true のときだけ存在・乗れる床
        SwitchBlockOff   // switchOn_ == false のときだけ存在・乗れる床
    };

    struct FragileState {
        bool  armed = false; // 踏んだ/頭突いた
        float t = 0.0f;      // armedしてからの経過時間
        bool  gone = false;  // 壊れていなくなった
    };
    struct RegenState {
        float respawn = 0.0f; // 復活までの経過
    };

    // ===== 外部参照 =====
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;
    SceneManager *sceneManager_ = nullptr;

    // ===== 描画リソース =====
    Camera camera_;

    Model  playerModel_;
    Model  mdlSolid_;
    Model  mdlFragileAny_;
    Model  mdlFragileTop_;
    Model  mdlFragileBottom_;
    Model  mdlRegen_;
    Model  mdlSpring_;
    Model  mdlSpike_;

    // --- スイッチ関連 ---
    // 押すスイッチ本体(ボタン)。ON表示用とOFF表示用の2つ。
    Model  mdlSwitchOn_;        // 青い押しスイッチ本体
    Model  mdlSwitchOff_;       // 赤い押しスイッチ本体

    // スイッチ連動の足場ブロック。ON時に出る足場とOFF時に出る足場を別モデルにできるように用意。
    Model  mdlSwitchBlockOn_;   // ON状態で存在する足場 (青床など)
    Model  mdlSwitchBlockOff_;  // OFF状態で存在する足場 (赤床など)

    Model  mdlJumpOnly_;

    // -------- テクスチャ保持用（SRVリソース） --------
    Microsoft::WRL::ComPtr<ID3D12Resource> texPlayer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSolid_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileAny_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileTop_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileBottom_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texRegen_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpring_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpike_;

    // スイッチ本体ON/OFF
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOff_;

    // 足場ON/OFF
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchBlockOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchBlockOff_;

    Microsoft::WRL::ComPtr<ID3D12Resource> texJumpOnly_;

    // -------- SRV heap slots (あくまで例。ヒープ側でかぶらないようにユニークにする) --------
    static constexpr UINT kSrvIndex_Player = 1;
    static constexpr UINT kSrvIndex_Solid = 2;
    static constexpr UINT kSrvIndex_FragileAny = 3;
    static constexpr UINT kSrvIndex_FragileTop = 4;
    static constexpr UINT kSrvIndex_FragileBottom = 5;
    static constexpr UINT kSrvIndex_Regen = 6;
    static constexpr UINT kSrvIndex_Spring = 7;
    static constexpr UINT kSrvIndex_Spike = 8;

    static constexpr UINT kSrvIndex_SwitchOn = 9;   // スイッチ本体ON
    static constexpr UINT kSrvIndex_SwitchOff = 10;  // スイッチ本体OFF

    static constexpr UINT kSrvIndex_SwitchBlockOn = 11;  // 足場ON
    static constexpr UINT kSrvIndex_SwitchBlockOff = 12;  // 足場OFF

    static constexpr UINT kSrvIndex_JumpOnly = 13;

    // ===== マップ =====
    Tile         grid_[kMapH][kMapW]{};
    FragileState frag_[kMapH][kMapW]{};
    RegenState   regen_[kMapH][kMapW]{};

    bool  switchOn_ = false; // スイッチON/OFF状態
    int   spawnTx_ = 2;
    int   spawnTy_ = 2;
    float xOffset_ = 0.0f;   // マップX座標オフセット(中央寄せ)

    // ===== プレイヤ =====
    Transform         playerTr_{};     // posはAABB左下を指すイメージ
    DirectX::XMFLOAT3 vel_{0,0,0};

    float pw_ = 0.99f;   // プレイヤー幅
    float ph_ = 0.99f;   // プレイヤー高さ

    bool  onGround_ = false;

    // 入力安定用
    int  coyoteCounter_ = 0;
    int  jumpBuffer_ = 0;
    unsigned char keyPrev_[256]{};
    bool KeyPressed_(uint8_t dik);

    // ステージ管理
    int stageId_ = 1; // このシーンが扱うステージ番号
    int maxStageCount_ = 10; // 総ステージ数（遷移用）

    // クリア管理
    bool  cleared_ = false;
    float elapsedTime_ = 0.0f; // プレイ時間
    float finalTime_ = 0.0f; // クリア確定時のタイム

    // ===== ユーティリティ =====
    static inline bool InMap(int tx, int ty) {
        return tx >= 0 && ty >= 0 && tx < kMapW && ty < kMapH;
    }

    // world <-> tile index
    inline int   ToTx(float wx) const { return (int)std::floor((wx - xOffset_) / kTile); }
    inline int   ToTy(float wy) const { return kMapH - 1 - (int)std::floor(wy / kTile); }
    inline float TyToWorldY(int ty) const { return (float)(kMapH - 1 - ty) * kTile; }

    static bool IsFragile(Tile t);
    static bool IsSpring(Tile t);
    bool IsBlockingAt(int tx, int ty) const;

    void ResetGrid();
    void BuildSample();

    bool SaveCSV(const std::string &path) const;
    bool LoadCSV(const std::string &path);
    void ClampSpawnToSafe();

    // 壊れる床のarmed開始
    void ArmFragile_(int tx, int ty);

    // 初期状態スナップショット（死亡リセット用）
    Tile         initialGrid_[kMapH][kMapW]{};
    FragileState initialFrag_[kMapH][kMapW]{};
    RegenState   initialRegen_[kMapH][kMapW]{};
    bool         initialSwitchOn_ = false;
    int          initialSpawnTx_ = 0;
    int          initialSpawnTy_ = 0;

    // 判定便利
    static inline bool OverlapXY(const AABB &a, float bx, float by, float bw, float bh) {
        return a.x < bx + bw && a.x + a.w > bx &&
            a.y < by + bh && a.y + a.h > by;
    }
    AABB PlayerAabbFull_() const { return {playerTr_.pos.x, playerTr_.pos.y, pw_, ph_}; }

    // ===== 物理 =====
    void ResolveHorizontal_();       // 横方向の衝突処理
    void ResolveVertical_(float dt); // 縦方向＋ギミック＋死亡判定

    // リスタート（死んだりしたとき）
    void ResetStageAll_();

    // ===== テクスチャ(SRV)読み込み =====
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);

    static std::wstring Widen_(const std::string &u8);

    // 描画本体（背景とマップとプレイヤー）
    void DrawBackgroundAndStage_();

    // 壊れ床の点滅アルファ
    float FragileBlinkFactor_(int tx, int ty) const;
};
