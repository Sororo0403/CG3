#pragma once
#include <Windows.h>

#include <DbgHelp.h>
#include <filesystem>
#include <string>
#include <strsafe.h>

namespace CrashHandler {

/// <summary>
/// ../Generated/Outputs/<config>/Dumps を返す
/// </summary>
inline std::filesystem::path GetDumpDirectory() {
    namespace fs = std::filesystem;

    fs::path exe = fs::current_path();
    fs::path base = exe / "../Generated/Outputs" / CONFIG_NAME;
    fs::path dumpDir = base / "Dumps";

    fs::create_directories(dumpDir);
    return dumpDir;
}

/// <summary>
/// ミニダンプ作成
/// </summary>
inline LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS *exception) {
    namespace fs = std::filesystem;

    fs::path dumpDir = GetDumpDirectory();

    // 日付でファイル名作成
    SYSTEMTIME time{};
    GetLocalTime(&time);

    wchar_t fileName[256];
    StringCchPrintfW(fileName, 256, L"crash_%04d-%02d%02d-%02d%02d%02d.dmp",
                     time.wYear, time.wMonth, time.wDay, time.wHour,
                     time.wMinute, time.wSecond);

    fs::path fullPath = dumpDir / fileName;

    HANDLE file =
        CreateFileW(fullPath.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("[CrashHandler] Failed to create dump file.\n");
        return EXCEPTION_EXECUTE_HANDLER;
    }

    MINIDUMP_EXCEPTION_INFORMATION info{};
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = exception;
    info.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                      MiniDumpNormal, &info, nullptr, nullptr);

    CloseHandle(file);

    OutputDebugStringA("[CrashHandler] Dump written.\n");
    return EXCEPTION_EXECUTE_HANDLER;
}

/// <summary>
/// 未処理例外ハンドラの登録
/// </summary>
inline void Install() {
    SetUnhandledExceptionFilter(ExceptionFilter);
}

} // namespace CrashHandler
