#include "StringUtility.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace StringUtility {
/// <summary>
/// ASCII文字列を小文字へ変換する。
/// </summary>
std::string ToLower(const std::string& text)
{
    std::string lowered = text; // 小文字化した結果
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lowered;
}

/// <summary>
/// ASCII文字列を大文字へ変換する。
/// </summary>
std::string ToUpper(const std::string& text)
{
    std::string uppered = text; // 大文字化した結果
    std::transform(uppered.begin(), uppered.end(), uppered.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return uppered;
}

/// <summary>
/// 文字列が指定した接頭辞で始まるかを確認する。
/// </summary>
bool StartsWith(const std::string& text, const std::string& prefix)
{
    if (prefix.size() > text.size()) {
        return false;
    }

    return std::equal(prefix.begin(), prefix.end(), text.begin());
}

/// <summary>
/// 文字列が指定した接尾辞で終わるかを確認する。
/// </summary>
bool EndsWith(const std::string& text, const std::string& suffix)
{
    if (suffix.size() > text.size()) {
        return false;
    }

    return std::equal(suffix.rbegin(), suffix.rend(), text.rbegin());
}

/// <summary>
/// 文字列に指定した文字列が含まれるかを確認する。
/// </summary>
bool Contains(const std::string& text, const std::string& keyword)
{
    return text.find(keyword) != std::string::npos;
}

/// <summary>
/// 文字列の前後の空白文字を取り除く。
/// </summary>
std::string Trim(const std::string& text)
{
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }); // 先頭側の空白を除いた開始位置
    if (first == text.end()) {
        return std::string();
    }

    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base(); // 末尾側の空白を除いた終了位置
    return std::string(first, last);
}

/// <summary>
/// 文字列を指定した区切り文字で分割する。
/// </summary>
std::vector<std::string> Split(const std::string& text, char delimiter, bool keepEmpty)
{
    std::vector<std::string> parts; // 分割した文字列一覧
    size_t start = 0; // 現在の要素の開始位置

    while (start <= text.size()) {
        const size_t delimiterPosition = text.find(delimiter, start); // 次の区切り文字位置
        const size_t end = delimiterPosition == std::string::npos ? text.size() : delimiterPosition; // 現在の要素の終了位置
        std::string part = text.substr(start, end - start); // 分割された1要素
        if (keepEmpty || !part.empty()) {
            parts.push_back(part);
        }

        if (delimiterPosition == std::string::npos) {
            break;
        }
        start = delimiterPosition + 1;
    }

    return parts;
}

/// <summary>
/// 文字列内の指定文字列をすべて置換する。
/// </summary>
std::string ReplaceAll(const std::string& text, const std::string& from, const std::string& to)
{
    if (from.empty()) {
        return text;
    }

    std::string replaced = text; // 置換後の文字列
    size_t position = 0; // 次に検索を開始する位置
    while ((position = replaced.find(from, position)) != std::string::npos) {
        replaced.replace(position, from.length(), to);
        position += to.length();
    }
    return replaced;
}
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