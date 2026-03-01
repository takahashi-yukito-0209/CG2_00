#include "StringUtility.h"
#include <Windows.h> 
#include <stdexcept>
#include <string>

namespace StringUtility {
/// <summary>
/// std::string から std::wstring へ変換する
/// </summary>
std::wstring ConvertString(const std::string& str)
{
    // 引数が空文字列の場合は空のワイド文字列を返す
    int requiredLength = MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.length(),
        nullptr, 0);

    // 変換に失敗した場合は空のワイド文字列を返す
    if (requiredLength == 0) {
        return L"";
    }

    // ワイド文字列を格納するためのバッファを確保
    std::wstring wstr(requiredLength, 0);

    // 変換を実行
    MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.length(),
        wstr.data(), requiredLength);

    // ワイド文字列を返す
    return wstr;
}

/// <summary>
/// std::wstring から std::string へ変換する
/// </summary>
std::string ConvertString(const std::wstring& wstr)
{
    // 引数が空文字列の場合は空の文字列を返す
    if (wstr.empty()) {
        return "";
    }

    // 変換に必要なバイト数を取得
    int requiredLength = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(),
        nullptr,
        0,
        nullptr,
        nullptr);

    // 変換に失敗した場合は空の文字列を返す
    if (requiredLength == 0) {
        return "";
    }

    // 変換後の文字列を格納するためのバッファを確保
    std::string str(requiredLength, '\0');

    // 変換を実行
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(),
        str.data(),
        requiredLength,
        nullptr,
        nullptr);

    return str; // ← 不要な resize は絶対にしない
}
} // namespace StringUtility