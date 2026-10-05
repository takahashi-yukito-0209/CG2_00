#include "JsonUtility.h"

#include <cmath>
#include <limits>

namespace JsonUtility {
namespace {
/// <summary>
/// JSON数値を有限のfloat値へ変換し、範囲外なら出力を変更しない。
/// </summary>
bool ReadFloatValue(const nlohmann::json& number, float& value)
{
    if (!number.is_number()) {
        return false;
    }

    const double parsedValue = number.get<double>(); // 変換前のJSON数値
    if (!std::isfinite(parsedValue) || std::abs(parsedValue) > (std::numeric_limits<float>::max)()) {
        return false;
    }

    value = static_cast<float>(parsedValue);
    return true;
}
} // namespace

/// <summary>
/// 指定したJSONオブジェクトを取得する。
/// </summary>
bool ExtractObjectSection(const nlohmann::json& object, const char* name, nlohmann::json& section)
{
    const auto member = object.find(name); // 読み取るJSONメンバー
    if (member == object.end() || !member->is_object()) {
        return false;
    }
    section = *member;
    return true;
}

/// <summary>
/// 指定した名前の文字列値を読み取る。
/// </summary>
bool ExtractString(const nlohmann::json& object, const char* name, std::string& value)
{
    const auto member = object.find(name); // 読み取るJSONメンバー
    if (member == object.end() || !member->is_string()) {
        return false;
    }
    value = member->get<std::string>();
    return true;
}

/// <summary>
/// 指定した名前の有限のfloat値を読み取る。
/// </summary>
bool ExtractFloat(const nlohmann::json& object, const char* name, float& value)
{
    const auto member = object.find(name); // 読み取るJSONメンバー
    return member != object.end() && ReadFloatValue(*member, value);
}

/// <summary>
/// 指定した名前のuint32_t値を範囲を確認して読み取る。
/// </summary>
bool ExtractUint(const nlohmann::json& object, const char* name, uint32_t& value)
{
    const auto member = object.find(name); // 読み取るJSONメンバー
    if (member == object.end() || !member->is_number_integer()) {
        return false;
    }
    if (!member->is_number_unsigned() && member->get<int64_t>() < 0) {
        return false;
    }

    const uint64_t parsedValue = member->get<uint64_t>(); // 範囲確認前の非負整数
    if (parsedValue > (std::numeric_limits<uint32_t>::max)()) {
        return false;
    }
    value = static_cast<uint32_t>(parsedValue);
    return true;
}

/// <summary>
/// 指定した名前のVector3配列を読み取り、全成分が有効な場合だけ反映する。
/// </summary>
bool ExtractVector3(const nlohmann::json& object, const char* name, Math::Vector3& value)
{
    const auto member = object.find(name); // 読み取るJSON配列
    if (member == object.end() || !member->is_array() || member->size() != 3) {
        return false;
    }
    Math::Vector3 parsedValue {}; // 全成分の検証が終わるまで保持する値
    if (!ReadFloatValue((*member)[0], parsedValue.x) || !ReadFloatValue((*member)[1], parsedValue.y) || !ReadFloatValue((*member)[2], parsedValue.z)) {
        return false;
    }
    value = parsedValue;
    return true;
}

/// <summary>
/// 指定した名前のVector4配列を読み取り、全成分が有効な場合だけ反映する。
/// </summary>
bool ExtractVector4(const nlohmann::json& object, const char* name, Math::Vector4& value)
{
    const auto member = object.find(name); // 読み取るJSON配列
    if (member == object.end() || !member->is_array() || member->size() != 4) {
        return false;
    }
    Math::Vector4 parsedValue {}; // 全成分の検証が終わるまで保持する値
    if (!ReadFloatValue((*member)[0], parsedValue.x) || !ReadFloatValue((*member)[1], parsedValue.y) || !ReadFloatValue((*member)[2], parsedValue.z) || !ReadFloatValue((*member)[3], parsedValue.w)) {
        return false;
    }
    value = parsedValue;
    return true;
}

} // namespace JsonUtility
