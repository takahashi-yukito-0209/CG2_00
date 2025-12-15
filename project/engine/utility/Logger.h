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

// 現在の最小出力レベルを設定する
void SetLevel(Level level);

// ログファイルを設定する（空文字でファイル出力を無効化）
bool SetLogFile(const std::string& filePath);

// レベル付きログ出力
void Log(Level level, const std::string& message);

// 互換性のための既存API
void Log(const std::string& message);

// レベル別の簡易ログ出力関数
inline void Debug(const std::string& msg) { Log(Level::Debug, msg); }
inline void Info(const std::string& msg) { Log(Level::Info, msg); }
inline void Warn(const std::string& msg) { Log(Level::Warn, msg); }
inline void Error(const std::string& msg) { Log(Level::Error, msg); }

} // namespace Logger
