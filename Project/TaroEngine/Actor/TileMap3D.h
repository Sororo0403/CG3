#pragma once
#include "TileTypes.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "Transform.h"
#include <vector>
#include <string>

/// <summary>
/// 3Dタイルマップ（論理はNovice版/CSV互換）
/// </summary>
class TileMap3D {
public:
    static constexpr int   kMapW = 26;
    static constexpr int   kMapH = 15;
    static constexpr float kTile = 1.0f;

    // Novice版の48px/60fpsを 1.0タイルスケールに換算
    static constexpr float kGravity = 0.55f / 48.0f * 60.0f;
    static constexpr float kMoveGround = 5.0f / 48.0f;
    static constexpr float kMoveAir = 3.0f / 48.0f;
    static constexpr float kJumpVy = -11.0f / 48.0f * 60.0f;
    static constexpr float kSpringVy = -18.0f / 48.0f * 60.0f;
    static constexpr float kMaxFallVy = 18.0f / 48.0f * 60.0f;

    // スキン幅
    static constexpr float kSkinY = 0.01f;
    static constexpr float kSkinX = 2.0f / 48.0f;

    // ジャンプ安定化
    static constexpr int kCoyoteMaxFrames = 6;
    static constexpr int kJumpBufferFrames = 6;

public:
    void ResetGrid();
    void BuildSample();

    // CSV互換
    bool SaveCSV(const std::string &path) const;
    bool LoadCSV(const std::string &path);
    bool CreateSnapshot(const std::string &baseCsvPath) const;

    // 位置→タイル座標（XZ平面）
    static inline int  ToTx(float px) { return (int)floorf(px / kTile); }
    static inline int  ToTy(float pz) { return (int)floorf(pz / kTile); }
    static inline bool InMap(int tx, int ty) { return (tx >= 0 && ty >= 0 && tx < kMapW && ty < kMapH); }

    // 属性/判定
    static bool IsFragile(Tile t);
    static bool IsSpring(Tile t);
    static bool IsSolidLikeBase(Tile t, bool switchOn);
    bool IsBlockingAt(int tx, int ty) const;

    // 進行
    void StepStates(float dt);
    int  CountRemainingFragile() const;

    // スイッチ
    bool GetSwitch() const { return switchOn_; }
    void ToggleSwitch() { switchOn_ = !switchOn_; }

    // 生成位置
    int  GetSpawnTx() const { return spawnTx_; }
    int  GetSpawnTy() const { return spawnTy_; }
    void SetSpawnTile(int tx, int ty);
    void ClampSpawnToSafe();

    // 編集
    void SetTile(int tx, int ty, Tile t);
    Tile GetTile(int tx, int ty) const { return grid_[ty][tx]; }
    FragileState &Frag(int tx, int ty) { return fragile_[ty][tx]; }
    const FragileState &Frag(int tx, int ty) const { return fragile_[ty][tx]; }

    // 描画（1キューブモデル＋色替え）
    void Draw(class ModelRenderer *renderer, ID3D12GraphicsCommandList *cmd, const Model &cubeModel) const;

    // 左上(0,0)のXオフセット（2Dの左右余白相当）
    float GetXOffset() const { return xOffset_; }
    void  SetXOffset(float v) { xOffset_ = v; }

private:
    static unsigned TileRGBA(Tile t, bool switchOn, bool blink);

private:
    std::vector<std::vector<Tile>>         grid_{kMapH, std::vector<Tile>(kMapW, Tile::Empty)};
    std::vector<std::vector<FragileState>> fragile_{kMapH, std::vector<FragileState>(kMapW)};
    std::vector<std::vector<RegenState>>   regen_{kMapH, std::vector<RegenState>(kMapW)};

    int  spawnTx_ = 2;
    int  spawnTy_ = 2;
    bool switchOn_ = false;

    float xOffset_ = 0.0f; // ワールドXの開始位置
};
