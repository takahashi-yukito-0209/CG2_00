#pragma once

#include <string>

/// <summary>
/// 文字列コード変換ユーティリティ
/// </summary>
namespace StringUtility {

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