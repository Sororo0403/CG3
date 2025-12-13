#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

class TextureManager;
class TextureDropQueue;

/// <summary>
/// ImGui 上でテクスチャを管理するためのエディタ。
/// ・ファイルドロップ
/// ・テクスチャ一覧表示
/// ・管理リストからの削除
/// を行う。
/// Sprite や描画処理には一切関与しない。
/// </summary>
class TextureEditor {
  public:
    TextureEditor(TextureManager *textureManager, TextureDropQueue *dropQueue);

    /// <summary>
    /// 毎フレーム呼ぶ更新処理（DropQueue 消費）
    /// </summary>
    void Update();

    /// <summary>
    /// ImGui 描画
    /// </summary>
    void DrawImGui();

    uint32_t Get(const std::string &name) const;
    bool Exists(const std::string &name) const;
    void Clear();

  private:
    uint32_t LoadInternal(const std::string &name, const std::string &path);

  private:
    TextureManager *textureManager_ = nullptr;
    TextureDropQueue *dropQueue_ = nullptr;

    // 論理名 → textureId
    std::unordered_map<std::string, uint32_t> nameToId_;
};
