#pragma once
#include "ILogger.h"
#include <string_view>

class OutputLogger : public ILogger {
public:
	/// <summary>
	/// デストラクタ
	/// </summary>
	~OutputLogger() override = default;

	/// <summary>
	/// 指定されたログレベルとメッセージをVisualStudioの出力ウィンドウへ出力
	/// </summary>
	/// <param name="logLevel">出力するログの重大度を示すレベル</param>
	/// <param name="message">出力するメッセージ文字列</param>
	void Log(LogLevel logLevel, std::string_view message) override;
};
