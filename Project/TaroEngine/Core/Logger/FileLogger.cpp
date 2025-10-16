#include "FileLogger.h"
#include <chrono>
#include <format>
#include <filesystem>
#include <cassert>
#include "StringUtility.h"

FileLogger::~FileLogger() {
	if (stream_.is_open()) {
		stream_.flush();
		stream_.close();
	}
}

void FileLogger::SetFilePath(const std::string &filePath) {
	std::scoped_lock lock(mutex_);

	// すでに開いている場合はいったん閉じる
	if (stream_.is_open()) {
		stream_.flush();
		stream_.close();
	}

	// 親ディレクトリを確認して存在しなければ作成
	std::filesystem::path dir = std::filesystem::path(filePath).parent_path();
	if (!dir.empty()) {
		if (!std::filesystem::exists(dir)) {
			std::filesystem::create_directories(dir);
		}
	}

	// ファイルを追記モードで開く
	stream_.open(filePath, std::ios::app);
	assert(stream_.is_open() && "Failed to open log file");
}

void FileLogger::Log(LogLevel logLevel, std::string_view message) {
	if (!stream_.is_open()) {
		return;
	}

	// ログメッセージを整形
	std::string formatted = LogUtility::Format(logLevel, message);

	// スレッドセーフに出力
	std::scoped_lock lock(mutex_);
	stream_ << formatted << std::endl;
}

void FileLogger::Log(LogLevel logLevel, std::wstring_view message) {
	if (!stream_.is_open()) return;

	// ワイド→UTF-8 にしてファイルへ
	std::wstring formattedW = LogUtility::FormatW(logLevel, message);
	std::string  utf8 = StringUtility::W2U8(formattedW);

	std::scoped_lock lock(mutex_);
	stream_.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
	stream_ << "\n";
}