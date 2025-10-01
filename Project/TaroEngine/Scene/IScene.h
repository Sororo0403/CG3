#pragma once
#include "EngineContext.h"

/// <summary>
/// シーンの共通インターフェイス。<br/>
/// すべてのシーンはこのインターフェイスを実装する必要がある。
/// </summary>
class IScene {
public:
    /// <summary>
    /// デストラクタ（仮想）。<br/>
    /// 派生クラスの破棄時に正しく解放されるようにする。
    /// </summary>
    virtual ~IScene() = default;

    /// <summary>
    /// シーンの初期化処理。<br/>
    /// エンジン側の共有リソース（デバイス、共通パイプライン等）を受け取って準備する。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    virtual void Initialize(const EngineContext &engine) = 0;

    /// <summary>
    /// 毎フレームの更新処理。<br/>
    /// 入力やアニメーション、ゲームロジックを進行させる。
    /// </summary>
    /// <param name="dt">経過時間（秒）。</param>
    virtual void Update(float dt) = 0;

    /// <summary>
    /// 描画処理。<br/>
    /// エンジンのコマンドリストなどを利用して、シーン内のオブジェクトを描画する。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    /// <param name="rc">描画コンテキスト（コマンドリストやターゲット情報）。</param>
    virtual void Draw(const EngineContext &engine, const RenderContext &rc) = 0;

    /// <summary>
    /// 終了処理。<br/>
    /// シーン固有のリソースを解放する。
    /// </summary>
    virtual void Finalize() = 0;
};
