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

struct AABB { float x, y; float w, h; };

class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void Finalize() override;

private:
    // ===== エディタ状態 / UI =====
    bool editorOn_ = false;                 // F1 でトグル
    int  paletteSel_ = 0;                   // 0..11=タイル, 12=Spawn
    int  hoverTx_ = -1, hoverTy_ = -1;      // マウス下のタイル
    bool uiVisible_ = true;                 // HUD 表示トグル

    // マウス→タイル変換（Z=0 平面）
    bool PickTileUnderMouse_(int &outTx, int &outTy, float *outWx = nullptr, float *outWy = nullptr) const;

    // ImGui エディタ（※ImGuiは Draw() 中からのみ呼ぶ）
    void EditorUI_();

    // ===== 基本定数（1タイル=1.0f / 1フレーム単位） =====
    static constexpr int   kMapW = 26;
    static constexpr int   kMapH = 15;
    static constexpr float kTile = 1.0f;

    // 物理チューニング
    static constexpr float kGravity    = 0.55f / 48.0f;
    static constexpr float kMoveGround = 5.0f  / 48.0f;
    static constexpr float kMoveAir    = 3.0f  / 48.0f;
    static constexpr float kJumpVy     = 11.0f / 48.0f;
    static constexpr float kSpringVy   = 18.0f / 48.0f;
    static constexpr float kMaxFallVy  = -18.0f / 48.0f;

    // スキン
    static constexpr float kSkinY = 0.01f;
    static constexpr float kSkinX = 2.0f / 48.0f;

    // ジャンプ安定化
    static constexpr int kCoyoteMaxFrames  = 6;
    static constexpr int kJumpBufferFrames = 6;

    // ===== タイル =====
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
    struct FragileState { bool armed = false; float t = 0.0f; bool gone = false; };
    struct RegenState { float respawn = 0.0f; };

    // ===== 参照 =====
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;

    // ===== 描画 =====
    Camera camera_;

    // プレイヤーモデル
    Model  playerModel_;

    // タイル用モデル種類
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

    // --- テクスチャ（寿命保持 & SRVインデックス固定）
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

    // SRV インデックス割り当て（グローバルSRVヒープ内の場所を固定）
    static constexpr UINT kSrvIndex_ImGuiFont      = 0; // ImGui が使う想定
    static constexpr UINT kSrvIndex_Player         = 1;
    static constexpr UINT kSrvIndex_Solid          = 2;
    static constexpr UINT kSrvIndex_FragileAny     = 3;
    static constexpr UINT kSrvIndex_FragileTop     = 4;
    static constexpr UINT kSrvIndex_FragileBottom  = 5;
    static constexpr UINT kSrvIndex_Regen          = 6;
    static constexpr UINT kSrvIndex_Spring         = 7;
    static constexpr UINT kSrvIndex_Spike          = 8;
    static constexpr UINT kSrvIndex_Switch         = 9;
    static constexpr UINT kSrvIndex_SwitchOn       = 10;
    static constexpr UINT kSrvIndex_SwitchOff      = 11;
    static constexpr UINT kSrvIndex_JumpOnly       = 12;

    // ===== マップ（XY平面 / CSV互換） =====
    Tile         grid_[kMapH][kMapW]{};
    FragileState frag_[kMapH][kMapW]{};
    RegenState   regen_[kMapH][kMapW]{};
    bool  switchOn_ = false;
    int   spawnTx_ = 2, spawnTy_ = 2;   // タイル座標
    float xOffset_ = 0.0f;              // 左端のX座標（中央寄せ）

    // ===== プレイヤ（左下基準） =====
    Transform         playerTr_{};
    DirectX::XMFLOAT3 vel_{0,0,0};  // x:左右  y:上下  zは使わない
    float pw_ = 1.0f, ph_ = 1.0f;
    bool  onGround_ = false;

    // ===== 入力補助 =====
    int  coyoteCounter_ = 0;
    int  jumpBuffer_ = 0;
    unsigned char keyPrev_[256]{};
    bool KeyPressed_(uint8_t dik);

    // ===== ユーティリティ =====
    static inline bool InMap(int tx, int ty) { return tx >= 0 && ty >= 0 && tx < kMapW && ty < kMapH; }

    // ワールド<->タイル（CSV互換：行tyは上→下。ワールドYは上が+）
    inline int   ToTx(float wx) const { return (int)floorf((wx - xOffset_) / kTile); }
    inline int   ToTy(float wy) const { return kMapH - 1 - (int)std::floor(wy / kTile); }
    inline float TyToWorldY(int ty) const { return (float)(kMapH - 1 - ty) * kTile; }

    // 属性
    static bool IsFragile(Tile t);
    static bool IsSpring(Tile t);
    bool IsBlockingAt(int tx, int ty) const;

    // マップ生成/CSV
    void ResetGrid();
    void BuildSample();
    bool SaveCSV(const std::string &path) const;
    bool LoadCSV(const std::string &path);
    bool CreateSnapshot(const std::string &baseCsvPath) const;
    void ClampSpawnToSafe();

    // 物理
    static inline bool OverlapXY(const AABB &a, float bx, float by, float bw, float bh) {
        return a.x < bx + bw && a.x + a.w > bx && a.y < by + bh && a.y + a.h > by;
    }
    AABB PlayerAabbX_() const;
    AABB PlayerAabbFull_() const { return {playerTr_.pos.x, playerTr_.pos.y, pw_, ph_}; }

    // ===== 画像→SRV ロード（sRGB） =====
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);

    // 文字列ユーティリティ
    static std::wstring Widen_(const std::string &u8);
};
