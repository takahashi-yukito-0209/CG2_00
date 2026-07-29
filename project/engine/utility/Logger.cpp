#include "Logger.h"
#include "FileUtility.h"
#include <Windows.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>

namespace Logger {

static Level s_minLevel = Level::Info; // 現在の最小出力ログレベル
static std::mutex s_mutex; // ログ出力を保護するミューテックス
static std::ofstream s_logFile; // 通常ログファイルストリーム
static std::ofstream s_errorLogFile; // Warn/Error 専用ログファイルストリーム
static std::string s_logFilePath; // 通常ログファイルのパス
static std::string s_errorLogFilePath; // Warn/Error 専用ログファイルのパス

/// <summary>
/// ログファイルを開く。空文字の場合はファイル出力を無効にする。
/// </summary>
static bool OpenLogFile(std::ofstream& stream, std::string& currentPath, const std::string& filePath)
{
    if (stream.is_open()) {
        stream.close();
    }

    currentPath.clear();
    if (filePath.empty()) {
        return true;
    }

    const std::string parentDirectory = FileUtility::GetParentDirectory(filePath); // ログファイルの親ディレクトリ
    if (!parentDirectory.empty() && !FileUtility::CreateDirectoryIfNeeded(parentDirectory)) {
        return false;
    }

    stream.open(filePath.c_str(), std::ios::out | std::ios::app);
    if (!stream.is_open()) {
        return false;
    }

    currentPath = FileUtility::NormalizePath(filePath);
    return true;
}

/// <summary>
/// 現在時刻のタイムスタンプ文字列を取得する。
/// </summary>
static std::string GetTimestamp()
{
    using namespace std::chrono;

    const auto now = system_clock::now(); // 現在時刻
    const std::time_t time = system_clock::to_time_t(now); // time_t 形式の現在時刻
    std::tm localTime {}; // ローカル時刻
    localtime_s(&localTime, &time);

    char dateTimeText[64] = {}; // 秒までの時刻文字列
    std::strftime(dateTimeText, sizeof(dateTimeText), "%Y-%m-%d %H:%M:%S", &localTime);

    const auto millisecond = duration_cast<milliseconds>(now.time_since_epoch()) % 1000; // ミリ秒
    char output[80] = {}; // ミリ秒付きタイムスタンプ
    sprintf_s(output, "%s.%03d", dateTimeText, static_cast<int>(millisecond.count()));

    return std::string(output);
}

/// <summary>
/// 出力用のログ文字列を作成する。
/// </summary>
static std::string FormatLog(Level level, const std::string& message)
{
    std::ostringstream stream; // ログ整形用ストリーム
    stream << "[" << GetTimestamp() << "] [" << ToString(level) << "] " << message;
    return stream.str();
}

const char* ToString(Level level)
{
    switch (level) {
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO";
    case Level::Warn:
        return "WARN";
    case Level::Error:
        return "ERROR";
    }

    return "UNKNOWN";
}

void SetLevel(Level level)
{
    std::lock_guard<std::mutex> lock(s_mutex); // ログ設定を保護するロック
    s_minLevel = level;
}

bool SetLogFile(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(s_mutex); // ログ設定を保護するロック
    return OpenLogFile(s_logFile, s_logFilePath, filePath);
}

bool SetErrorLogFile(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(s_mutex); // ログ設定を保護するロック
    return OpenLogFile(s_errorLogFile, s_errorLogFilePath, filePath);
}

void Log(Level level, const std::string& message)
{
    if (level < s_minLevel) {
        return;
    }

    const std::string output = FormatLog(level, message); // 出力するログ文字列

    std::lock_guard<std::mutex> lock(s_mutex); // ログ出力を保護するロック
    OutputDebugStringA((output + "\n").c_str());

    if (s_logFile.is_open()) {
        s_logFile << output << std::endl;
        s_logFile.flush();
    }

    const bool isErrorLevel = level == Level::Warn || level == Level::Error; // 異常系ログかどうか
    const bool isSameFile = !s_logFilePath.empty() && s_logFilePath == s_errorLogFilePath; // 通常ログと専用ログが同一か
    if (isErrorLevel && s_errorLogFile.is_open() && !isSameFile) {
        s_errorLogFile << output << std::endl;
        s_errorLogFile.flush();
    }
}

void LogWithLocation(Level level, const std::string& message, const char* file, int line, const char* function)
{
    std::ostringstream stream; // 発生場所付きログの整形用ストリーム
    stream << message << " (" << file << ":" << line << " " << function << ")";
    Log(level, stream.str());
}

void Log(const std::string& message)
{
    Log(Level::Info, message);
}

} // namespace Logger