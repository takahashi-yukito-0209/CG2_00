#pragma once

#include <string>
#include <vector>

/// <summary>
/// 文字列コード変換ユーティリティ
/// </summary>
namespace StringUtility {


/// <summary>
/// ASCII文字列を小文字へ変換する。
/// </summary>
std::string ToLower(const std::string& text);

/// <summary>
/// ASCII文字列を大文字へ変換する。
/// </summary>
std::string ToUpper(const std::string& text);

/// <summary>
/// 文字列が指定した接頭辞で始まるかを確認する。
/// </summary>
bool StartsWith(const std::string& text, const std::string& prefix);

/// <summary>
/// 文字列が指定した接尾辞で終わるかを確認する。
/// </summary>
bool EndsWith(const std::string& text, const std::string& suffix);

/// <summary>
/// 文字列に指定した文字列が含まれるかを確認する。
/// </summary>
bool Contains(const std::string& text, const std::string& keyword);

/// <summary>
/// 文字列の前後の空白文字を取り除く。
/// </summary>
std::string Trim(const std::string& text);

/// <summary>
/// 文字列を指定した区切り文字で分割する。
/// </summary>
std::vector<std::string> Split(const std::string& text, char delimiter, bool keepEmpty = false);

/// <summary>
/// 文字列内の指定文字列をすべて置換する。
/// </summary>
std::string ReplaceAll(const std::string& text, const std::string& from, const std::string& to);

/// <summary>
/// UTF-8文字列をワイド文字列へ変換する。失敗した場合は false を返す。
/// </summary>
bool TryConvertString(const std::string& str, std::wstring& out);

/// <summary>
/// ワイド文字列をUTF-8文字列へ変換する。失敗した場合は false を返す。
/// </summary>
bool TryConvertString(const std::wstring& wstr, std::string& out);

/// <summary>
/// std::string から std::wstring へ変換する。失敗した場合は空文字列を返す。
/// </summary>
std::wstring ConvertString(const std::string& str);

/// <summary>
/// std::wstring から std::string へ変換する。失敗した場合は空文字列を返す。
/// </summary>
std::string ConvertString(const std::wstring& wstr);

}