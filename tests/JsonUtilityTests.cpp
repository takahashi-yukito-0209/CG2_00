#include "engine/utility/JsonUtility.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
/// <summary>
/// 条件を満たさない場合にテストを失敗させる。
/// </summary>
void Require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// 文字列の保存と読み込みで特殊文字が維持されることを確認する。
/// </summary>
void TestStringRoundTrip()
{
    const std::string original = "quote: \" slash: \\ newline:\n tab:\t"; // 往復させる特殊文字列
    const nlohmann::json source = { { "description", original } }; // 書き出す設定のJSON
    const nlohmann::json document = nlohmann::json::parse(source.dump(2)); // 書き出したJSONのパース結果
    std::string restored; // 読み戻した文字列
    Require(JsonUtility::ExtractString(document, "description", restored), "String extraction failed");
    Require(restored == original, "String round trip changed escaped characters");
}

/// <summary>
/// 指数表記と整数境界を確認し、不正な値で出力が変わらないことを確認する。
/// </summary>
void TestNumericBoundaries()
{
    const nlohmann::json document = nlohmann::json::parse(R"({"small":1e-5,"huge":1e100,"max":4294967295,"overflow":4294967296,"negative":-1,"fraction":2.5,"text":"1"})"); // 境界値のJSON
    float floatValue = 3.0f; // 読み取り結果と失敗時の保持値
    Require(JsonUtility::ExtractFloat(document, "small", floatValue) && std::abs(floatValue - 1e-5f) < 1e-10f, "Exponent notation was not preserved");
    floatValue = 3.0f;
    Require(!JsonUtility::ExtractFloat(document, "huge", floatValue) && floatValue == 3.0f, "Float overflow changed output");
    Require(!JsonUtility::ExtractFloat(document, "text", floatValue) && floatValue == 3.0f, "String was accepted as float");

    uint32_t uintValue = 7; // 整数の読み取り結果
    Require(JsonUtility::ExtractUint(document, "max", uintValue) && uintValue == UINT32_MAX, "Maximum uint32 value failed");
    uintValue = 7;
    for (const char* name : { "overflow", "negative", "fraction", "missing" }) { // 拒否する整数値のキー
        Require(!JsonUtility::ExtractUint(document, name, uintValue) && uintValue == 7, "Invalid integer changed output");
    }
}

/// <summary>
/// 配列の全成分を検証し、不正な配列で一部だけ書き換わらないことを確認する。
/// </summary>
void TestVectorValidation()
{
    const nlohmann::json document = nlohmann::json::parse(R"({"valid":[1,2e-3,-3],"mixed":[9,"bad",8],"short":[1,2],"long":[1,2,3,4],"color":[1,0.5,0.25,1]})"); // 配列検証用のJSON
    Math::Vector3 vector { 4.0f, 5.0f, 6.0f }; // 失敗時に保持するベクトル
    for (const char* name : { "mixed", "short", "long", "missing" }) { // 拒否する配列のキー
        Require(!JsonUtility::ExtractVector3(document, name, vector), "Invalid Vector3 accepted");
        Require(vector.x == 4.0f && vector.y == 5.0f && vector.z == 6.0f, "Vector3 was partially updated");
    }
    Require(JsonUtility::ExtractVector3(document, "valid", vector) && vector.x == 1.0f && vector.z == -3.0f, "Valid Vector3 failed");
    Math::Vector4 color {}; // 読み戻した色
    Require(JsonUtility::ExtractVector4(document, "color", color) && color.w == 1.0f, "Valid Vector4 failed");
}

/// <summary>
/// 旧形式とカテゴリ分割形式の設定を読み取れることを確認する。
/// </summary>
void TestSectionCompatibility()
{
    const nlohmann::json legacy = { { "radius", 2.0f } }; // ルート直下へ値を置く旧形式
    nlohmann::json section = legacy; // カテゴリがない場合の読み取り元
    Require(!JsonUtility::ExtractObjectSection(legacy, "emitter", section), "Missing section unexpectedly found");
    float radius = 0.0f; // 読み取り結果
    Require(JsonUtility::ExtractFloat(section, "radius", radius) && radius == 2.0f, "Legacy fallback failed");
    const nlohmann::json nested = { { "emitter", { { "radius", 3.0f } } } }; // カテゴリ分割形式
    Require(JsonUtility::ExtractObjectSection(nested, "emitter", section), "Nested section failed");
    Require(JsonUtility::ExtractFloat(section, "radius", radius) && radius == 3.0f, "Nested value failed");
}

/// <summary>
/// 実際のGPUエミッタープリセットを読み取り、保存後も設定値が維持されることを確認する。
/// </summary>
void TestExistingPresets(const std::filesystem::path& directory)
{
    size_t presetCount = 0; // 検証したプリセット数
    for (const auto& entry : std::filesystem::directory_iterator(directory)) { // 検証対象の設定ファイル
        if (entry.path().extension() != ".json") {
            continue;
        }
        std::ifstream input(entry.path()); // プリセットの読み取り元
        const nlohmann::json document = nlohmann::json::parse(input); // 既存プリセットの設定
        nlohmann::json emitter; // 発生設定カテゴリ
        Require(JsonUtility::ExtractObjectSection(document, "emitter", emitter), "Preset emitter section missing");
        float radius = 0.0f; // 発生半径
        uint32_t count = 0; // 発生数
        Math::Vector3 position {}; // 発生位置
        Math::Vector4 color {}; // 発生色
        Require(JsonUtility::ExtractFloat(emitter, "radius", radius), "Preset radius invalid");
        Require(JsonUtility::ExtractUint(emitter, "count", count), "Preset count invalid");
        Require(JsonUtility::ExtractVector3(emitter, "translate", position), "Preset position invalid");
        Require(JsonUtility::ExtractVector4(emitter, "colorMin", color), "Preset color invalid");
        Require(nlohmann::json::parse(document.dump()) == document, "Preset serialization changed values");
        ++presetCount;
    }
    Require(presetCount > 0, "No presets tested");
    std::cout << "Presets tested: " << presetCount << '\n';
}
} // namespace

/// <summary>
/// JSON変換と既存プリセットの互換性テストを実行する。
/// </summary>
int main(int argc, char** argv)
{
    try {
        TestStringRoundTrip();
        TestNumericBoundaries();
        TestVectorValidation();
        TestSectionCompatibility();
        TestExistingPresets(argc > 1 ? argv[1] : "project/resources/effects");
        std::cout << "JSON utility tests passed\n";
        return 0;
    } catch (const std::exception& exception) { // テスト失敗の理由
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
