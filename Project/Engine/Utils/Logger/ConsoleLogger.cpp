#include "ConsoleLogger.h"
#include "LogFormatter.h"
#include "Time/TimeUtil.h"
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#endif

void ConsoleLogger::Write(LogLevel level, const std::string &message,
                          const char *file, int line) {
  // 現在時刻
  std::string time = TimeUtil::NowTimeString();

  // 書式済み文字列
  std::string formatted =
      LogFormatter::Format(time, level, message, file, line);

  // 標準出力
  std::cout << formatted << std::endl;

#ifdef _WIN32
  // Visual Studio の出力ウィンドウにも出す
  OutputDebugStringA((formatted + "\n").c_str());
#endif
}
