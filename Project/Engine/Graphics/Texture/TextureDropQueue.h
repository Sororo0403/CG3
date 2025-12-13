#pragma once
#include <queue>
#include <string>

/// <summary>
/// OS からドロップされたテクスチャパスを一時的に保持するキュー。
/// Win32 層と Editor 層の橋渡しを行うだけの軽量クラス。
/// </summary>
class TextureDropQueue {
  public:
    /// <summary>
    /// パスをキューに積む
    /// </summary>
    void Push(const std::string &path);

    /// <summary>
    /// パスを1つ取り出す
    /// </summary>
    /// <param name="outPath">取り出したパス</param>
    /// <returns>取り出せたら true</returns>
    bool Pop(std::string &outPath);

    /// <summary>
    /// キューをクリア
    /// </summary>
    void Clear();

    // Getter
    bool Empty() const {
        return queue_.empty();
    };

  private:
    std::queue<std::string> queue_;
};
