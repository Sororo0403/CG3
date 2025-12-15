#pragma once

#include <queue>
#include <string>
#include <mutex>

/// <summary>
/// OS からドロップされたテクスチャパスを一時的に保持するキュー。
/// Win32 層と Editor 層の橋渡しを行うだけの軽量クラス。
/// スレッドセーフ。
/// </summary>
class TextureDropQueue {
  public:
    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    TextureDropQueue() = default;

    /// <summary>
    /// デストラクタ
    /// ※ 終了時クラッシュ防止のため、何もしない
    /// </summary>
    ~TextureDropQueue() = default;

    /// <summary>
    /// パスをキューに積む（スレッドセーフ）
    /// </summary>
    void Push(const std::string &path);

    /// <summary>
    /// パスを1つ取り出す（スレッドセーフ）
    /// </summary>
    /// <param name="outPath">取り出したパス</param>
    /// <returns>取り出せたら true</returns>
    bool Pop(std::string &outPath);

    /// <summary>
    /// キューを安全にクリアする（スレッドセーフ）
    /// </summary>
    void Clear();

    /// <summary>
    /// キューが空かどうか（スレッドセーフ）
    /// </summary>
    bool Empty() const;

  private:
    mutable std::mutex mtx_;
    std::queue<std::string> queue_;
};
