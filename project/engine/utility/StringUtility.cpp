#include "StringUtility.h"
#include <Windows.h>
#include <string>

namespace StringUtility {
/// <summary>
/// UTF-8文字列をワイド文字列へ変換する。失敗した場合は false を返す。
/// </summary>
bool TryConvertString(const std::string& str, std::wstring& out)
{
    out.clear();
    if (str.empty()) {
        return true;
    }

    int requiredLength = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        str.c_str(),
        static_cast<int>(str.length()),
        nullptr,
        0); // 変換後に必要なワイド文字数
    if (requiredLength == 0) {
        return false;
    }

    std::wstring converted(requiredLength, L'\0'); // 変換結果を格納する文字列
    int convertedLength = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        str.c_str(),
        static_cast<int>(str.length()),
        converted.data(),
        requiredLength); // 実際に変換できたワイド文字数
    if (convertedLength == 0) {
        return false;
    }

    out = std::move(converted);
    return true;
}

/// <summary>
/// ワイド文字列をUTF-8文字列へ変換する。失敗した場合は false を返す。
/// </summary>
bool TryConvertString(const std::wstring& wstr, std::string& out)
{
    out.clear();
    if (wstr.empty()) {
        return true;
    }

    int requiredLength = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        nullptr,
        0,
        nullptr,
        nullptr); // 変換後に必要なバイト数
    if (requiredLength == 0) {
        return false;
    }

    std::string converted(requiredLength, '\0'); // 変換結果を格納する文字列
    int convertedLength = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        converted.data(),
        requiredLength,
        nullptr,
        nullptr); // 実際に変換できたバイト数
    if (convertedLength == 0) {
        return false;
    }

    out = std::move(converted);
    return true;
}

/// <summary>
/// std::string から std::wstring へ変換する。失敗した場合は空文字列を返す。
/// </summary>
std::wstring ConvertString(const std::string& str)
{
    std::wstring converted; // 変換結果を受け取るワイド文字列
    if (!TryConvertString(str, converted)) {
        return L"";
    }
    return converted;
}

/// <summary>
/// std::wstring から std::string へ変換する。失敗した場合は空文字列を返す。
/// </summary>
std::string ConvertString(const std::wstring& wstr)
{
    std::string converted; // 変換結果を受け取るUTF-8文字列
    if (!TryConvertString(wstr, converted)) {
        return "";
    }
    return converted;
}

} // namespace StringUtility