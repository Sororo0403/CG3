#pragma once
#include <string>
#include <windows.h>

namespace StringUtil {

inline std::string ToUTF8(const std::wstring &wstr) {
  int sizeNeeded =
      WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr,
                          0, nullptr, nullptr);

  std::string result(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(),
                      result.data(), sizeNeeded, nullptr, nullptr);

  return result;
}

} // namespace StringUtil