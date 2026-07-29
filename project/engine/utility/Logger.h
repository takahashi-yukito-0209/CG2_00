#pragma once

#include <string>

/// <summary>
/// ログ出力に関するユーティリティ名前空間。
/// </summary>
namespace Logger {

/// <summary>
/// ログの重要度を表す列挙型。
/// </summary>
enum class Level {
    Debug = 0,
    Info,
    Warn,
    Error,
};

/// <summary>
/// ログレベルを文字列へ変換する。
/// </summary>
const char* ToString(Level level);

/// <summary>
/// 現在の最小出力ログレベルを設定する。
/// </summary>
void SetLevel(Level level);

/// <summary>
/// 通常ログの出力先ファイルを設定する。空文字を渡すとファイル出力を無効にする。
/// </summary>
bool SetLogFile(const std::string& filePath);

/// <summary>
/// Warn/Error 専用ログの出力先ファイルを設定する。空文字を渡すと専用ファイル出力を無効にする。
/// </summary>
bool SetErrorLogFile(const std::string& filePath);

/// <summary>
/// レベル付きログを出力する。
/// </summary>
void Log(Level level, const std::string& message);

/// <summary>
/// 発生場所付きのレベル付きログを出力する。
/// </summary>
void LogWithLocation(Level level, const std::string& message, const char* file, int line, const char* function);

/// <summary>
/// 互換性維持用の Info ログを出力する。
/// </summary>
void Log(const std::string& message);

// レベル別の簡易 API
inline void Debug(const std::string& msg) { Log(Level::Debug, msg); }
inline void Info(const std::string& msg) { Log(Level::Info, msg); }
inline void Warn(const std::string& msg) { Log(Level::Warn, msg); }
inline void Error(const std::string& msg) { Log(Level::Error, msg); }

} // namespace Logger