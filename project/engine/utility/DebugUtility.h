#pragma once

#include <Windows.h>
#include <string>

/// <summary>
/// デバッグ補助に関するユーティリティ名前空間。
/// </summary>
namespace DebugUtility {

/// <summary>
/// HRESULT を 16進文字列へ変換する。
/// </summary>
std::string ToHexString(HRESULT result);

/// <summary>
/// HRESULT とメッセージを組み合わせたログ文字列を作成する。
/// </summary>
std::string FormatHResult(HRESULT result, const std::string& message);

/// <summary>
/// HRESULT が失敗を表す場合にエラーログを出力する。成功時は true を返す。
/// </summary>
bool CheckHResult(HRESULT result, const std::string& message);

/// <summary>
/// HRESULT の内容をエラーログとして出力する。
/// </summary>
void LogHResult(HRESULT result, const std::string& message);

/// <summary>
/// HRESULT と発生場所を含むログ文字列を作成する。
/// </summary>
std::string FormatHResultWithLocation(HRESULT result, const std::string& message, const char* file, int line, const char* function);

/// <summary>
/// HRESULT が失敗を表す場合に発生場所付きのエラーログを出力する。成功時は true を返す。
/// </summary>
bool CheckHResultWithLocation(HRESULT result, const std::string& message, const char* file, int line, const char* function);

/// <summary>
/// HRESULT の内容を発生場所付きのエラーログとして出力する。
/// </summary>
void LogHResultWithLocation(HRESULT result, const std::string& message, const char* file, int line, const char* function);
} // namespace DebugUtility

/// <summary>
/// HRESULT が失敗した場合にファイル名、行番号、関数名付きでログを出力する。
/// </summary>
#define MYENGINE_CHECK_HRESULT(result, message) DebugUtility::CheckHResultWithLocation((result), (message), __FILE__, __LINE__, __func__)