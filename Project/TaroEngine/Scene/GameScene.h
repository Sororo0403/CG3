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
/// ステージエディタ付き2D横アクション（タイル＋物理＋ギミック）
/// </summary>
class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void Finalize() override;

private:
    // ===== エディタ状態 / UI =====
    bool editorOn_ = false;
    int  paletteSel_ = 0;            // 0..11=タイル, 12=Spawn
    int  hoverTx_ = -1, hoverTy_ = -1;
    bool uiVisible_ = true;

    bool PickTileUnderMouse_(int &outTx, int &outTy, float *outWx = nullptr, float *outWy = nullptr) const;
    void EditorUI_();

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

    // スキン（タイルとわずかに浮かせる/離すマージン）
    static constexpr float kSkinY = 0.01f;
    static constexpr float kSkinX = 2.0f / 48.0f;

    // 接地・側面判定に使う最小オーバーラップ
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
        bool  armed = false; // 踏んだり頭突きした
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

    // SRV heap slots (一致させること)
    static constexpr UINT kSrvIndex_ImGuiFont = 0;
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
    int   spawnTx_ = 2, spawnTy_ = 2;
    float xOffset_ = 0.0f; // マップをX方向に中央寄せするオフセット

    // ===== プレイヤ =====
    Transform         playerTr_{};     // posはAABBの左下
    DirectX::XMFLOAT3 vel_{0,0,0};
    float pw_ = 1.0f; // プレイヤーAABB幅
    float ph_ = 1.0f; // プレイヤーAABB高さ
    bool  onGround_ = false;

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
    bool CreateSnapshot(const std::string &baseCsvPath) const;
    void ClampSpawnToSafe();

    // AABB
    static inline bool OverlapXY(const AABB &a, float bx, float by, float bw, float bh) {
        return a.x < bx + bw && a.x + a.w > bx && a.y < by + bh && a.y + a.h > by;
    }
    AABB PlayerAabbFull_() const { return {playerTr_.pos.x, playerTr_.pos.y, pw_, ph_}; }

    // ===== 物理（スイープ） =====
    void ResolveHorizontal_();      // 1フレームぶんのX移動を確定（めり込まない）
    void ResolveVertical_(float dt);// 1フレームぶんのY移動を確定（めり込まない＋接地処理など）

    // ===== テクスチャ読み込み (SRV割り当て含む) =====
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);

    static std::wstring Widen_(const std::string &u8);
};
