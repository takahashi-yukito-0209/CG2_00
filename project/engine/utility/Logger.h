#pragma once

#include <string>

/// <summary>
/// ログ出力ユーティリティ
/// </summary>
namespace Logger {

/// ログレベル
enum class Level {
    Debug = 0,
    Info,
    Warn,
    Error,
};

/// <summary>
/// 現在の最小出力レベルを設定する
/// </summary>
void SetLevel(Level level);

/// <summary>
/// ログファイルを設定する（空文字でファイル出力を無効化）
/// </summary>
bool SetLogFile(const std::string& filePath);

/// <summary>
/// 異常系（Warn/Error）専用のログファイルを設定する（空文字で無効化）
/// </summary>
bool SetErrorLogFile(const std::string& filePath);

/// <summary>
/// レベル付きログ出力
/// </summary>
void Log(Level level, const std::string& message);

/// <summary>
/// 互換性のための既存API
/// </summary>
void Log(const std::string& message);

// --- レベル別の簡易API ---
inline void Debug(const std::string& msg) { Log(Level::Debug, msg); }
inline void Info(const std::string& msg) { Log(Level::Info, msg); }
inline void Warn(const std::string& msg) { Log(Level::Warn, msg); }
inline void Error(const std::string& msg) { Log(Level::Error, msg); }

} // namespace Logger
