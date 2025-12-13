#pragma once

#include <cstdint>

class SpriteManager;
class TextureEditor;

/// <summary>
/// スプライト編集用エディタ。
/// SpriteManager が管理する Sprite を対象に、
/// Hierarchy / Inspector 形式で編集 UI（ImGui）を提供する。
/// </summary>
class SpriteEditor {
  public:
    /// <summary>
    /// SpriteEditor を生成する。
    /// 編集対象となる SpriteManager TextureEditor を外部から注入する。
    /// </summary>
    SpriteEditor(SpriteManager *spriteManager, TextureEditor *textureEditor);

    /// <summary>
    /// 初期化処理。
    /// 選択状態など、エディタ内部の状態をリセットする。
    /// </summary>
    void Initialize();

    /// <summary>
    /// ImGui を用いたスプライト編集 UI を描画する。
    /// 毎フレーム呼び出されることを想定している。
    /// </summary>
    void DrawImGui();

  private:
    SpriteManager *spriteManager_ = nullptr;
    TextureEditor *textureEditor_ = nullptr;

    uint32_t selectedId_ = 0;
};
