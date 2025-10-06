#pragma once
#include "EngineContext.h"
#include "RenderContext.h"

class IScene {
public:
    /// <summary>
    /// 仮想デストラクタ。派生クラスの破棄を正しく行う。
    /// </summary>
    virtual ~IScene() = default;

    /// <summary>
    /// シーンを初期化し、共有リソースを受け取る。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    virtual void Initialize(const EngineContext &engine) = 0;

    /// <summary>
    /// シーンを更新し、ゲームロジックを進行させる。
    /// </summary>
    /// <param name="dt">経過時間（秒）。</param>
    virtual void Update(float dt) = 0;

    /// <summary>
    /// シーンを描画し、コマンドリストを用いてオブジェクトを描く。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    /// <param name="rc">描画コンテキスト。</param>
    virtual void Draw(const EngineContext &engine, const RenderContext &rc) = 0;

    /// <summary>
    /// シーン固有のリソースを解放する。
    /// </summary>
    virtual void Finalize() = 0;
};
