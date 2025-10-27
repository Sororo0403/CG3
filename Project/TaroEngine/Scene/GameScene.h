#pragma once
#define NOMINMAX

#include "EngineContext.h"
#include "RenderContext.h"
#include "IScene.h"
#include "DirectXCommon.h"
#include "Camera.h"
#include "Model.h"
#include "Transform.h"

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
///
/// ★演出コンセプト：深夜の工事現場の高所足場
///  - 背景にクレーン/投光器
///  - 各ブロックは仮設足場＋支柱＋立入禁止テープ
///  - 壊れる足場はヒビ＋警告サイン
///  - スパイクはバリケード
///  - 全体を囲う鉄骨フレーム
/// </summary>
class GameScene : public IScene {
public:
    GameScene(int stageId = 1) : stageId_(stageId) {}
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void Finalize() override;

private:
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

    // スキン（ブロックとほんの紙一枚だけ離すマージン）
    static constexpr float kSkinY = 0.005f;
    static constexpr float kSkinX = 0.005f;

    // 接地・側面判定に使う最小オーバーラップ量
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
        Switch,
        SwitchBlockOn,
        SwitchBlockOff,
    };

    struct FragileState {
        bool  armed = false; // 踏んだ/頭突いた
        float t = 0.0f;      // armedからの経過
        bool  gone = false;  // 壊れて今は消えてる
    };
    struct RegenState {
        float respawn = 0.0f; // 復活経過時間
    };

    // ===== 外部参照 =====
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;

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
    Model  mdlSwitch_;
    Model  mdlSwitchBlockOn_;
    Model  mdlSwitchBlockOff_;
    Model  mdlJumpOnly_;

    Microsoft::WRL::ComPtr<ID3D12Resource> texPlayer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSolid_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileAny_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileTop_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileBottom_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texRegen_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpring_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpike_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOff_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texJumpOnly_;

    // SRV heap slots（DirectXCommon側のSRVヒープの固定スロット割り当て想定）
    static constexpr UINT kSrvIndex_Player = 1;
    static constexpr UINT kSrvIndex_Solid = 2;
    static constexpr UINT kSrvIndex_FragileAny = 3;
    static constexpr UINT kSrvIndex_FragileTop = 4;
    static constexpr UINT kSrvIndex_FragileBottom = 5;
    static constexpr UINT kSrvIndex_Regen = 6;
    static constexpr UINT kSrvIndex_Spring = 7;
    static constexpr UINT kSrvIndex_Spike = 8;
    static constexpr UINT kSrvIndex_Switch = 9;
    static constexpr UINT kSrvIndex_SwitchOn = 10;
    static constexpr UINT kSrvIndex_SwitchOff = 11;
    static constexpr UINT kSrvIndex_JumpOnly = 12;

    // ===== マップ =====
    Tile         grid_[kMapH][kMapW]{};
    FragileState frag_[kMapH][kMapW]{};
    RegenState   regen_[kMapH][kMapW]{};

    bool  switchOn_ = false;
    int   spawnTx_ = 2;
    int   spawnTy_ = 2;
    float xOffset_ = 0.0f; // マップをX方向に中央寄せするオフセット

    // ===== プレイヤ =====
    Transform         playerTr_{};     // posはAABBの左下
    DirectX::XMFLOAT3 vel_{0,0,0};

    // 固定AABBサイズ
    float pw_ = 1.0f;   // プレイヤー幅
    float ph_ = 0.99f;  // プレイヤー高さ

    bool  onGround_ = false;

    int stageId_ = 1; // どのステージか

    // 入力補助
    int  coyoteCounter_ = 0;
    int  jumpBuffer_ = 0;
    unsigned char keyPrev_[256]{};
    bool KeyPressed_(uint8_t dik);

    // ===== ユーティリティ =====
    static inline bool InMap(int tx, int ty) {
        return tx >= 0 && ty >= 0 && tx < kMapW && ty < kMapH;
    }

    // world <-> tile
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

    Tile          initialGrid_[kMapH][kMapW]{};
    FragileState  initialFrag_[kMapH][kMapW]{};
    RegenState    initialRegen_[kMapH][kMapW]{};
    bool          initialSwitchOn_ = false;
    int           initialSpawnTx_ = 0;
    int           initialSpawnTy_ = 0;

    // AABB
    static inline bool OverlapXY(const AABB &a, float bx, float by, float bw, float bh) {
        return a.x < bx + bw && a.x + a.w > bx && a.y < by + bh && a.y + a.h > by;
    }
    AABB PlayerAabbFull_() const { return {playerTr_.pos.x, playerTr_.pos.y, pw_, ph_}; }

    // ===== 物理（スイープ） =====
    void ResolveHorizontal_();       // 横衝突の安定化
    void ResolveVertical_(float dt); // 縦＋ギミック＋着地/頭ぶつけ

    void ResetStageAll_(); // 死亡やリセット時に最初の状態へ戻す

    // ===== テクスチャ読み込み (SRV割り当て含む) =====
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);

    static std::wstring Widen_(const std::string &u8);

    // 背景3Dをまとめて描く
    void DrawBackgroundAndStage_();
};
