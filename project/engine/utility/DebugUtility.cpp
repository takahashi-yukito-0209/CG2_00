#include "DebugUtility.h"
#include "Logger.h"
#include <iomanip>
#include <sstream>

namespace DebugUtility {

/// <summary>
/// HRESULT を 16進文字列へ変換する。
/// </summary>
std::string ToHexString(HRESULT result)
{
    std::ostringstream stream; // 16進文字列を作成するストリーム
    stream << "0x"
           << std::uppercase
           << std::hex
           << std::setw(8)
           << std::setfill('0')
           << static_cast<unsigned long>(result);
    return stream.str();
}

/// <summary>
/// HRESULT とメッセージを組み合わせたログ文字列を作成する。
/// </summary>
std::string FormatHResult(HRESULT result, const std::string& message)
{
    std::ostringstream stream; // ログ文字列を作成するストリーム
    stream << message << " HRESULT=" << ToHexString(result);
    return stream.str();
}

/// <summary>
/// HRESULT が失敗を表す場合にエラーログを出力する。成功時は true を返す。
/// </summary>
bool CheckHResult(HRESULT result, const std::string& message)
{
    if (SUCCEEDED(result)) {
        return true;
    }

    LogHResult(result, message);
    return false;
}

/// <summary>
/// HRESULT の内容をエラーログとして出力する。
/// </summary>
void LogHResult(HRESULT result, const std::string& message)
{
    Logger::Error(FormatHResult(result, message) + "\n");
}

/// <summary>
/// HRESULT と発生場所を含むログ文字列を作成する。
/// </summary>
std::string FormatHResultWithLocation(HRESULT result, const std::string& message, const char* file, int line, const char* function)
{
    std::ostringstream stream; // 発生場所付きログ文字列を作成するストリーム
    stream << FormatHResult(result, message) << " (" << file << ":" << line << " " << function << ")";
    return stream.str();
}

/// <summary>
/// HRESULT が失敗を表す場合に発生場所付きのエラーログを出力する。成功時は true を返す。
/// </summary>
bool CheckHResultWithLocation(HRESULT result, const std::string& message, const char* file, int line, const char* function)
{
    if (SUCCEEDED(result)) {
        return true;
    }

    LogHResultWithLocation(result, message, file, line, function);
    return false;
}

/// <summary>
/// HRESULT の内容を発生場所付きのエラーログとして出力する。
/// </summary>
void LogHResultWithLocation(HRESULT result, const std::string& message, const char* file, int line, const char* function)
{
    Logger::Error(FormatHResultWithLocation(result, message, file, line, function) + "\n");
}
} // namespace DebugUtility