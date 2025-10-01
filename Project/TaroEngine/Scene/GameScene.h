#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "Camera.h"      // ★ 追加

/// <summary>
/// 実際のゲーム用のシーン。<br/>
/// スプライトなどを管理し、更新・描画処理を行う。
/// </summary>
class GameScene : public IScene {
public:
    /// <summary>
    /// 初期化処理。<br/>
    /// エンジンから共有リソースを受け取り、DI（依存性注入）で利用する。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    void Initialize(const EngineContext &engine) override;

    /// <summary>
    /// ゲーム更新処理。<br/>
    /// 入力やアニメーション、オブジェクトの状態を更新する。
    /// </summary>
    /// <param name="dt">経過時間（秒）。</param>
    void Update(float dt) override;

    /// <summary>
    /// 描画処理。<br/>
    /// 共通のPSOを適用した後、スプライトを個別に描画する。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    /// <param name="rc">描画コンテキスト。</param>
    void Draw(const EngineContext &engine, const RenderContext &rc) override;

    /// <summary>
    /// 終了処理。<br/>
    /// ゲームシーン固有のリソースを解放する。
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// （任意）リサイズ通知がある場合に呼ぶ。<br/>
    /// カメラのアスペクト比を更新する。
    /// </summary>
    /// <param name="w">幅</param>
    /// <param name="h">高さ</param>
    void OnResize(uint32_t w, uint32_t h);

private:
    Sprite sprite_; // このシーンで使う単独スプライト
    Camera camera_; // ★ 3D カメラ

    // IMGUI 用一時値（ドラッグ操作をスムーズにするため保持）
    Vector3 camPos_{0.0f, 3.0f, -8.0f};
    Vector3 camTarget_{0.0f, 1.0f, 0.0f};
    float camFovDeg_ = 60.0f;
    float camNear_ = 0.1f;
    float camFar_ = 2000.0f;
};
