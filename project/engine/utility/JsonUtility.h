#pragma once

#include "MathTypes.h"
#include "externals/nlohmann/json.hpp"

#include <cstdint>
#include <string>

/// <summary>
/// 構造化JSONから値を読み取り、エンジンの型へ変換するユーティリティ。
/// </summary>
namespace JsonUtility {

/// <summary>
/// 指定したJSONオブジェクトを取得する。
/// </summary>
bool ExtractObjectSection(const nlohmann::json& object, const char* name, nlohmann::json& section);

/// <summary>
/// 指定した名前の文字列値を読み取る。
/// </summary>
bool ExtractString(const nlohmann::json& object, const char* name, std::string& value);

/// <summary>
/// 指定した名前のfloat値を読み取る。
/// </summary>
bool ExtractFloat(const nlohmann::json& object, const char* name, float& value);

/// <summary>
/// 指定した名前のuint32_t値を読み取る。
/// </summary>
bool ExtractUint(const nlohmann::json& object, const char* name, uint32_t& value);

/// <summary>
/// 指定した名前のVector3配列値を読み取る。
/// </summary>
bool ExtractVector3(const nlohmann::json& object, const char* name, Math::Vector3& value);

/// <summary>
/// 指定した名前のVector4配列値を読み取る。
/// </summary>
bool ExtractVector4(const nlohmann::json& object, const char* name, Math::Vector4& value);

} // namespace JsonUtility
