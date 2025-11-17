#pragma once

#include <string>

/// <summary>
/// 文字列コードユーティリティ
/// </summary>
namespace StringUtility {
/// <summary>
/// std::string から std::wstring へ変換する
/// </summary>
std::wstring ConvertString(const std::string& str);

/// <summary>
/// std::wstring から std::string へ変換する
/// </summary>
std::string ConvertString(const std::wstring& wstr);
}