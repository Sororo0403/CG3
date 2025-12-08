#pragma once
#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel { INFO, WARNING, ERROR, DEBUG };

class Logger {
public:
  /// <summary>
  /// インスタンスを取得
  /// </summary>
  /// <returns>インスタンス</returns>
  static Logger *GetInstance();

  /// <summary>
  /// 初期化処理
  /// </summary>
  /// <param name="filePath">出力先ファイルパス</param>
  void Initialize(const std::string &filePath = "log.txt");

  /// <summary>
  /// 指定されたログレベル・メッセージをログに書き込みます
  /// </summary>
  /// <param name="level">ログレベル</param>
  /// <param name="message">ログメッセージ</param>
  /// <param name="file">ログを出力したソースファイル名</param>
  /// <param name="line">ログを出力した行番号</param>
  void Write(LogLevel level, const std::string &message, const char *file,
             int line);

  // Logger
  void Info(const std::string &msg, const char *file, int line);
  void Warning(const std::string &msg, const char *file, int line);
  void Error(const std::string &msg, const char *file, int line);
  void Debug(const std::string &msg, const char *file, int line);

private:
  // シングルトン
  Logger() = default;
  ~Logger();

private:
  std::ofstream file_;
  std::mutex mtx_;
};

// マクロ
#define LOGGER_INFO(msg) Logger::GetInstance().Info(msg, __FILE__, __LINE__)
#define LOGGER_WARN(msg) Logger::GetInstance().Warning(msg, __FILE__, __LINE__)
#define LOGGER_ERROR(msg) Logger::GetInstance().Error(msg, __FILE__, __LINE__)
#define LOGGER_DEBUG(msg) Logger::GetInstance().Debug(msg, __FILE__, __LINE__)
