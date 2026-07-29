#pragma once

#include "MathTypes.h"

#include <cstdint>
#include <iosfwd>
#include <string>

/// <summary>
/// JSON形式の簡易的な読み書きを扱うユーティリティ。
/// </summary>
namespace JsonUtility {

/// <summary>
/// JSON文字列として出力できるように特殊文字をエスケープする。
/// </summary>
std::string EscapeString(const std::string& value);

/// <summary>
/// JSON文字列の1項目を書き出す。
/// </summary>
void WriteString(std::ostream& stream, const char* name, const std::string& value, const char* suffix);

/// <summary>
/// Vector3をJSON配列の1項目として書き出す。
/// </summary>
void WriteVector3(std::ostream& stream, const char* name, const Math::Vector3& value, const char* suffix);

/// <summary>
/// Vector4をJSON配列の1項目として書き出す。
/// </summary>
void WriteVector4(std::ostream& stream, const char* name, const Math::Vector4& value, const char* suffix);

/// <summary>
/// 指定したJSONオブジェクトの本文を切り出す。
/// </summary>
bool ExtractObjectSection(const std::string& text, const char* name, std::string& section);

/// <summary>
/// 指定した名前の文字列値を読み取る。
/// </summary>
bool ExtractString(const std::string& text, const char* name, std::string& value);

/// <summary>
/// 指定した名前のfloat値を読み取る。
/// </summary>
bool ExtractFloat(const std::string& text, const char* name, float& value);

/// <summary>
/// 指定した名前のuint32_t値を読み取る。
/// </summary>
bool ExtractUint(const std::string& text, const char* name, uint32_t& value);

/// <summary>
/// 指定した名前のVector3配列値を読み取る。
/// </summary>
bool ExtractVector3(const std::string& text, const char* name, Math::Vector3& value);

/// <summary>
/// 指定した名前のVector4配列値を読み取る。
/// </summary>
bool ExtractVector4(const std::string& text, const char* name, Math::Vector4& value);

} // namespace JsonUtility