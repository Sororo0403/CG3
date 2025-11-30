#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <Windows.h>

/// <summary>
/// 文字列ユーティリティ
/// </summary>
namespace StringUtility {

    /// <summary>
    /// wstring の配列を区切り文字で連結して1つの文字列にする。
    /// </summary>
    /// <param name="v">結合する文字列配列</param>
    /// <param name="sep">区切り文字</param>
    /// <returns>連結後の1つの wstring</returns>
    inline std::wstring WJoin(const std::vector<std::wstring> &v, wchar_t sep) {
        std::wstringstream ss;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) ss << sep;
            ss << v[i];
        }
        return ss.str();
    }

    /// <summary>
    /// ワイド文字列ビュー（std::wstring_view）をUTF-8エンコードされたstd::stringに変換します。
    /// </summary>
    /// <param name="w">変換するワイド文字列ビュー。</param>
    /// <returns>UTF-8エンコードされたstd::string。入力が空の場合は空文字列を返します。</returns>
    inline std::string W2U8(std::wstring_view w) {
        if (w.empty()) return {};
        int len = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string out(len, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), len, nullptr, nullptr);
        return out;
    }

} // namespace StringUtility
