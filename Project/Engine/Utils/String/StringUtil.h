#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

namespace StringUtil {

/// <summary>
/// UTF-16 ワイド文字列（std::wstring）を UTF-8 エンコードされた std::string に変換します。
/// Windows API（WideCharToMultiByte）を使用します。
/// </summary>
/// <param name="wstr">変換したい UTF-16 ワイド文字列</param>
/// <returns>UTF-8 エンコードされた std::string</returns>
inline std::string UTF16ToUTF8(const std::wstring &wstr) {
  if (wstr.empty()) {
    return std::string();
  }

  int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                       static_cast<int>(wstr.size()), nullptr,
                                       0, nullptr, nullptr);

  if (sizeNeeded <= 0) {
    return std::string();
  }

  std::string result(sizeNeeded, 0);

  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
                      result.data(), sizeNeeded, nullptr, nullptr);

  return result;
}

/// <summary>
/// UTF-8 文字列（std::string）を UTF-16 ワイド文字列（std::wstring）に変換します。
/// Windows API（MultiByteToWideChar）を使用します。
/// </summary>
/// <param name="str">変換したい UTF-8 文字列</param>
/// <returns>UTF-16 ワイド文字列（std::wstring）</returns>
inline std::wstring UTF8ToUTF16(const std::string &str) {
  if (str.empty()) {
    return std::wstring();
  }

  int sizeNeeded = MultiByteToWideChar(
      CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);

  if (sizeNeeded <= 0) {
    return std::wstring();
  }

  std::wstring result(sizeNeeded, 0);

  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
                      result.data(), sizeNeeded);

  return result;
}

} // namespace StringUtil
