#pragma once
#include <Windows.h>
#include <string>

namespace StringUtil {

/// <summary>
/// wstring → UTF-8 string
/// </summary>
inline std::string ToString(const std::wstring &ws) {
  if (ws.empty())
    return "";

  int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr,
                                nullptr);

  std::string result(len - 1, '\0');

  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, result.data(), len, nullptr,
                      nullptr);

  return result;
}

/// <summary>
/// UTF-8 string → wstring
/// </summary>
inline std::wstring ToWString(const std::string &s) {
  if (s.empty())
    return L"";

  int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);

  std::wstring result(len - 1, L'\0');

  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, result.data(), len);

  return result;
}

} // namespace StringUtil
