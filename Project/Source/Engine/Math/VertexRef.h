#pragma once

#include <cstddef>

struct VertexRef {
    int v; 
    int vt;
    int vn;

    /// <summary>
    /// 2つの VertexRef が同一のインデックス組を持つかどうかを比較します。
    /// </summary>
    /// <param name="rhs">比較対象の VertexRef 構造体。</param>
    /// <returns>true の場合、全ての要素 (v/vt/vn) が一致しています。</returns>
    bool operator==(const VertexRef &rhs) const = default;
};

struct VertexRefHash {
    /// <summary>
    /// VertexRef の内容 (v/vt/vn) からハッシュ値を計算します。
    /// </summary>
    /// <param name="r">ハッシュ計算対象の VertexRef。</param>
    /// <returns>VertexRef を識別するための size_t 型ハッシュ値。</returns>
    size_t operator()(const VertexRef &r) const noexcept {
        return (static_cast<size_t>(r.v) * 73856093u) ^
            (static_cast<size_t>(r.vt) * 19349663u) ^
            (static_cast<size_t>(r.vn) * 83492791u);
    }
};
