#pragma once

#include <string>

/// <summary>
/// ログ出力ユーティリティ
/// </summary>
namespace Logger {
/// <summary>
/// メッセージをデバッグコンソールに出力する
/// </summary>
void Log(const std::string& message);
}