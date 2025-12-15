#pragma once

class DirectXCommon;
class Input;
class WinApp;

/// <summary>
/// シーン基底クラス
/// </summary>
class Scene {
  public:
    virtual ~Scene() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    virtual void Initialize() = 0;

    /// <summary>
    /// 更新
    /// </summary>
    virtual void Update() = 0;

    /// <summary>
    /// 描画
    /// </summary>
    virtual void Draw() = 0;

    /// <summary>
    /// 終了判定
    /// </summary>
    virtual bool IsEnd() const {
        return false;
    }
};
