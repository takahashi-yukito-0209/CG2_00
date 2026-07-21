#include "JsonUtility.h"

#include <ostream>
#include <regex>

namespace JsonUtility {

/// <summary>
/// JSON文字列として出力できるように特殊文字をエスケープする。
/// </summary>
std::string EscapeString(const std::string& value)
{
    std::string escaped; // JSON出力用にエスケープした文字列
    escaped.reserve(value.size());
    for (char character : value) {
        if (character == '\\' || character == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

/// <summary>
/// JSON文字列の1項目を書き出す。
/// </summary>
void WriteString(std::ostream& stream, const char* name, const std::string& value, const char* suffix)
{
    stream << "  \"" << name << "\": \"" << EscapeString(value) << "\"" << suffix << "\n";
}

/// <summary>
/// Vector3をJSON配列の1項目として書き出す。
/// </summary>
void WriteVector3(std::ostream& stream, const char* name, const Math::Vector3& value, const char* suffix)
{
    stream << "  \"" << name << "\": [" << value.x << ", " << value.y << ", " << value.z << "]" << suffix << "\n";
}

/// <summary>
/// Vector4をJSON配列の1項目として書き出す。
/// </summary>
void WriteVector4(std::ostream& stream, const char* name, const Math::Vector4& value, const char* suffix)
{
    stream << "  \"" << name << "\": [" << value.x << ", " << value.y << ", " << value.z << ", " << value.w << "]" << suffix << "\n";
}

/// <summary>
/// 指定したJSONオブジェクトの本文を切り出す。
/// </summary>
bool ExtractObjectSection(const std::string& text, const char* name, std::string& section)
{
    const std::string key = std::string("\"") + name + "\""; // 検索するオブジェクト名
    const size_t keyPosition = text.find(key); // オブジェクト名の位置
    if (keyPosition == std::string::npos) {
        return false;
    }

    const size_t colonPosition = text.find(':', keyPosition + key.size()); // オブジェクト名直後の区切り位置
    if (colonPosition == std::string::npos) {
        return false;
    }

    const size_t objectStart = text.find('{', colonPosition + 1); // オブジェクト開始位置
    if (objectStart == std::string::npos) {
        return false;
    }

    uint32_t depth = 0; // ネストしているオブジェクトの深さ
    bool inString = false; // 文字列内を走査しているか
    bool escaping = false; // エスケープ文字の直後か
    for (size_t index = objectStart; index < text.size(); ++index) {
        const char character = text[index]; // 現在確認している文字
        if (inString) {
            if (escaping) {
                escaping = false;
            } else if (character == '\\') {
                escaping = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }

        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{') {
            ++depth;
            continue;
        }
        if (character == '}') {
            if (depth == 0) {
                return false;
            }
            --depth;
            if (depth == 0) {
                section = text.substr(objectStart + 1, index - objectStart - 1);
                return true;
            }
        }
    }

    return false;
}

/// <summary>
/// 指定した名前の文字列値を読み取る。
/// </summary>
bool ExtractString(const std::string& text, const char* name, std::string& value)
{
    const std::regex pattern(std::string("\\\"") + name + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\""); // 文字列値を取り出す正規表現
    std::smatch match; // 正規表現の一致結果
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }

    value = match[1].str();
    return true;
}

/// <summary>
/// 指定した名前のfloat値を読み取る。
/// </summary>
bool ExtractFloat(const std::string& text, const char* name, float& value)
{
    const std::regex pattern(std::string("\\\"") + name + "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)"); // float値を取り出す正規表現
    std::smatch match; // 正規表現の一致結果
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }

    value = std::stof(match[1].str());
    return true;
}

/// <summary>
/// 指定した名前のuint32_t値を読み取る。
/// </summary>
bool ExtractUint(const std::string& text, const char* name, uint32_t& value)
{
    const std::regex pattern(std::string("\\\"") + name + "\\\"\\s*:\\s*([0-9]+)"); // uint32_t値を取り出す正規表現
    std::smatch match; // 正規表現の一致結果
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }

    value = static_cast<uint32_t>(std::stoul(match[1].str()));
    return true;
}

/// <summary>
/// 指定した名前のVector3配列値を読み取る。
/// </summary>
bool ExtractVector3(const std::string& text, const char* name, Math::Vector3& value)
{
    const std::regex pattern(std::string("\\\"") + name + "\\\"\\s*:\\s*\\[\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*,\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*,\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*\\]"); // Vector3配列を取り出す正規表現
    std::smatch match; // 正規表現の一致結果
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }

    value.x = std::stof(match[1].str());
    value.y = std::stof(match[2].str());
    value.z = std::stof(match[3].str());
    return true;
}

/// <summary>
/// 指定した名前のVector4配列値を読み取る。
/// </summary>
bool ExtractVector4(const std::string& text, const char* name, Math::Vector4& value)
{
    const std::regex pattern(std::string("\\\"") + name + "\\\"\\s*:\\s*\\[\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*,\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*,\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*,\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*\\]"); // Vector4配列を取り出す正規表現
    std::smatch match; // 正規表現の一致結果
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }

    value.x = std::stof(match[1].str());
    value.y = std::stof(match[2].str());
    value.z = std::stof(match[3].str());
    value.w = std::stof(match[4].str());
    return true;
}

} // namespace JsonUtility
