#pragma once
#include "EngineContext.h"

/// <summary>
/// シーンの共通インターフェイス。
/// すべてのシーンはこのインターフェイスを実装する。
/// </summary>
class IScene {
public:
    virtual ~IScene() = default;

    /// <summary>
    /// 共有リソースを受け取って初期化
    /// </summary>
    virtual void Initialize(const EngineContext &engine) = 0;

    /// <summary>
    /// 更新処理
    /// </summary>
    virtual void Update(float dt) = 0;

    /// <summary>
    /// 描画処理
    /// </summary>
    virtual void Draw(const EngineContext &engine, const RenderContext &rc) = 0;

    /// <summary>
    /// 終了処理
    /// </summary>
    virtual void Finalize() = 0;
};
