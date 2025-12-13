#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
    struct Entry {
        std::string name;
        uint32_t textureId;
    };

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

    /// <summary>
    /// 管理をクリア
    /// </summary>
    void Clear();

    // Getter
    uint32_t GetTexture(const std::string &name) const;
    bool IsExists(const std::string &name) const;
    void GetEntries(std::vector<Entry> &out) const;

  private:
    uint32_t LoadInternal(const std::string &name, const std::string &path);

  private:
    TextureManager *textureManager_ = nullptr;
    TextureDropQueue *dropQueue_ = nullptr;

    std::unordered_map<std::string, uint32_t> nameToId_;
};
