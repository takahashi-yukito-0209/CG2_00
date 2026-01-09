#include "Logger.h"
#include <Windows.h>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <fstream>

namespace Logger {

// 現在の最小出力レベル
static Level s_minLevel = Level::Debug;
// ミューテックス
static std::mutex s_mutex;
// ログファイルストリーム
static std::ofstream s_logFile;
// 異常系ログファイルストリーム（Warn/Error）
static std::ofstream s_errorLogFile;

// レベルを文字列に変換
static const char* LevelToString(Level l) {
    // レベルを文字列に変換
    switch (l) {
    case Level::Debug:
        return "DEBUG"; // NOLINT(clang-diagnostic-switch-enum)
    case Level::Info:
        return "INFO"; // NOLINT(clang-diagnostic-switch-enum)
    case Level::Warn:
        return "WARN"; // NOLINT(clang-diagnostic-switch-enum)
    case Level::Error:
        return "ERROR"; // NOLINT(clang-diagnostic-switch-enum)
    }
    // 安全策: すべての経路で文字列を返す
    return "UNKNOWN";
}

// タイムスタンプ取得
static std::string GetTimestamp() {
    // 現在時刻を取得
    using namespace std::chrono;
    auto now = system_clock::now();

    // 文字列に変換
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);

    // 文字列フォーマット
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);

    // ミリ秒を追加
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    char out[80];
    sprintf_s(out, "%s.%03d", buf, static_cast<int>(ms.count()));

    return std::string(out);
}

// ログファイルを設定
bool SetLogFile(const std::string& filePath) {
    // ミューテックスで保護
    std::lock_guard<std::mutex> lk(s_mutex);
    if (s_logFile.is_open()) {
        s_logFile.close();
    }

    // 空文字ならファイル出力を無効化
    if (filePath.empty()) {
        return true;
    }

    // ファイルを開く
    s_logFile.open(filePath.c_str(), std::ios::out | std::ios::app);
    return s_logFile.is_open();
}

// 異常系（Warn/Error）専用ログファイルを設定
bool SetErrorLogFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lk(s_mutex);
    if (s_errorLogFile.is_open()) {
        s_errorLogFile.close();
    }

    if (filePath.empty()) {
        return true;
    }

    s_errorLogFile.open(filePath.c_str(), std::ios::out | std::ios::app);
    return s_errorLogFile.is_open();
}

// 最小出力レベルを設定
void SetLevel(Level level) {
    std::lock_guard<std::mutex> lk(s_mutex);
    s_minLevel = level;
}

// レベル付きログを出力
void Log(Level level, const std::string& message) {
    // レベルによるフィルタリング
    if (level < s_minLevel) {
        return;
    }

    // タイムスタンプ取得と整形
    std::string ts = GetTimestamp();
    // 出力フォーマット整形
    std::ostringstream oss;
    // 形式: [timestamp] [LEVEL] message
    oss << "[" << ts << "] [" << LevelToString(level) << "] " << message;
    // 出力文字列
    std::string out = oss.str();

    // ミューテックスで保護して出力
    {
        std::lock_guard<std::mutex> lk(s_mutex);
        OutputDebugStringA((out + "\n").c_str());
        // ファイル出力は異常系（Warn/Error）のみ
        if (level == Level::Warn || level == Level::Error) {
            if (s_errorLogFile.is_open()) {
                s_errorLogFile << out << std::endl;
                s_errorLogFile.flush();
            } else if (s_logFile.is_open()) {
                s_logFile << out << std::endl;
                s_logFile.flush();
            }
        }
    }
}

// 互換性のための既存API
void Log(const std::string& message) {
    Log(Level::Info, message);
}

} // namespace Logger
