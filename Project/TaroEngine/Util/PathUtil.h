#pragma once
#include <filesystem>

/// <summary>
/// パス関連のユーティリティ関数をまとめた名前空間。
/// <para>主に「Project」ディレクトリの親階層にある「Generated」フォルダへのパス操作を行う。</para>
/// </summary>
namespace PathUtil {

    /// <summary>
    /// 現在の作業ディレクトリ（例: Project）から1つ上の階層にある
    /// 「Generated」ディレクトリを取得します。存在しない場合は作成します。
    /// </summary>
    /// <returns>「Generated」ディレクトリの絶対パス。</returns>
    inline std::filesystem::path FindOrCreateGenerated() {
        namespace fs = std::filesystem;
        fs::path gen = fs::current_path().parent_path() / "Generated";
        fs::create_directories(gen);
        return gen;
    }

    /// <summary>
    /// 「Generated/Logs」ディレクトリを取得します。存在しない場合は作成します。
    /// </summary>
    /// <returns>「Generated/Logs」ディレクトリの絶対パス。</returns>
    inline std::filesystem::path EnsureLogsDir() {
        namespace fs = std::filesystem;
        fs::path logs = FindOrCreateGenerated() / "Logs";
        fs::create_directories(logs);
        return logs;
    }

    /// <summary>
    /// 既定のログファイルパス（Generated/Logs/app.log）を取得します。
    /// </summary>
    /// <param name="fileName">ログファイル名。省略時は "app.log"。</param>
    /// <returns>ログファイルの絶対パス。</returns>
    inline std::filesystem::path DefaultLogFilePath(const char *fileName = "app.log") {
        return EnsureLogsDir() / fileName;
    }

} // namespace PathUtil
