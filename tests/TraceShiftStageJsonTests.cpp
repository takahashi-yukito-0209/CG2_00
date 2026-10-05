#include "application/scenes/TraceShiftStageJson.h"
#include "application/scenes/PlaySceneEditorState.h"
#include "application/gimmicks/StageGimmickTransformUtility.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
/// <summary>
/// テスト条件が成立しない場合に失敗理由を通知する。
/// </summary>
void Check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// 不正データの読込失敗時に、既存の保存情報が変更されないことを確認する。
/// </summary>
void CheckRejected(const nlohmann::json& root)
{
    TraceShiftStageJson::StageData data; // 読込失敗後も維持すべき既存情報
    data.blocks.emplace_back().name = "keep block";
    data.gimmicks.emplace_back().id = "keep gimmick";
    std::string error; // 検証失敗の理由
    Check(!TraceShiftStageJson::Decode(root, data, error), "Invalid input was accepted");
    Check(!error.empty(), "Missing validation error");
    Check(data.blocks.size() == 1 && data.blocks[0].name == "keep block", "Blocks changed on failure");
    Check(data.gimmicks.size() == 1 && data.gimmicks[0].id == "keep gimmick", "Gimmicks changed on failure");
}

/// <summary>
/// 指定なし・一部指定・読み込み順序の違いでも初期配置を基準に解決することを確認する。
/// </summary>
void TestGimmickDefaults(const nlohmann::json& original)
{
    TraceShiftStageJson::StageData initial; // 初期配置のテスト基準
    std::string error; // 検証結果
    Check(TraceShiftStageJson::Decode(original, initial, error), "Default fixture failed to load");
    Check(initial.gimmicks.size() == TraceShiftStageJson::kGimmickIds.size(), "Default fixture has missing gimmicks");
    auto firstOverrides = initial.gimmicks; // 全配置を変更した最初の読み込み
    for (auto& layout : firstOverrides) {
        layout.translate.x += 20.0f;
        layout.scale.x = 0.0f;
        if (layout.hasUpperTranslate) {
            layout.upperTranslate.x += 20.0f;
        }
    }
    const auto first = TraceShiftStageJson::ResolveGimmickLayouts(initial.gimmicks, firstOverrides); // 最初の配置解決結果
    Check(first[0].translate.x != initial.gimmicks[0].translate.x, "Override was not applied");
    auto partial = initial.gimmicks[0]; // 次の読み込みで指定する1件だけの配置
    partial.translate.y += 10.0f;
    const auto second = TraceShiftStageJson::ResolveGimmickLayouts(initial.gimmicks, { partial }); // 前回の配置を参照しない解決結果
    Check(second[0].translate.y == partial.translate.y, "Partial override was lost");
    for (size_t index = 1; index < second.size(); ++index) {
        Check(second[index].translate.x == initial.gimmicks[index].translate.x
            && second[index].scale.x == initial.gimmicks[index].scale.x,
            "Omitted gimmick kept previous layout");
    }
    TraceShiftStageJson::StageData restored = initial; // 指定をすべて省略した配置
    restored.gimmicks = TraceShiftStageJson::ResolveGimmickLayouts(initial.gimmicks, {});
    Check(TraceShiftStageJson::Encode(restored) == original, "Empty override failed to restore defaults");
    auto reversed = firstOverrides; // JSON内の並び順を変えた同じ指定配置
    std::reverse(reversed.begin(), reversed.end());
    TraceShiftStageJson::StageData forward = initial; // 順方向の配置解決結果
    TraceShiftStageJson::StageData backward = initial; // 逆方向の配置解決結果
    forward.gimmicks = first;
    backward.gimmicks = TraceShiftStageJson::ResolveGimmickLayouts(initial.gimmicks, reversed);
    Check(TraceShiftStageJson::Encode(forward) == TraceShiftStageJson::Encode(backward), "Gimmick array order changed layout");
}

/// <summary>
/// 保存文字列の互換性と、不正な編集状態・UTF-8による保存拒否を確認する。
/// </summary>
void TestSerialization(const nlohmann::json& original)
{
    TraceShiftStageJson::StageData data; // 実ステージの保存対象
    std::string error; // 保存失敗理由
    std::string text = "keep output"; // 失敗時に保持する文字列
    Check(TraceShiftStageJson::Decode(original, data, error), "Serialization fixture failed to load");
    Check(TraceShiftStageJson::Serialize(data, text, error) && error.empty() && text == original.dump(4),
        "Serialized format changed");
    const auto checkInvalid = [&](const TraceShiftStageJson::StageData& invalid) {
        text = "keep output";
        Check(!TraceShiftStageJson::Serialize(invalid, text, error) && !error.empty() && text == "keep output",
            "Invalid stage was serialized or changed output");
    }; // 不正な保存スナップショットの拒否確認
    auto invalid = data; // 不正状態の検証用コピー
    invalid.blocks.clear();
    checkInvalid(invalid);
    invalid = data;
    for (auto& block : invalid.blocks) { // ゴールを削除するブロック
        block.goalMarker = false;
    }
    checkInvalid(invalid);
    invalid = data;
    invalid.blocks[0].translate.x = std::numeric_limits<float>::quiet_NaN();
    checkInvalid(invalid);
    invalid = data;
    invalid.blocks[0].color.w = std::numeric_limits<float>::infinity();
    checkInvalid(invalid);
    invalid = data;
    invalid.blocks[0].name = std::string(1, static_cast<char>(0xff));
    checkInvalid(invalid);
    invalid = data;
    invalid.gimmicks.push_back(invalid.gimmicks[0]);
    checkInvalid(invalid);
    invalid = data;
    invalid.gimmicks.back().hasUpperTranslate = false;
    checkInvalid(invalid);
}

/// <summary>
/// ゼロ縮小後の復元と、繰り返し編集によるスイッチ判定の累積誤差を確認する。
/// </summary>
void TestSwitchVolumeRestoration()
{
    const SwitchVolumeBasis basis { { 2, 1, 4 }, { 3, 2, 1 }, { 3, 3, 1 }, { 1, 2, 3 } }; // 変更されない初期設定
    Math::Vector3 center {}; // 編集後の判定中心
    Math::Vector3 halfSize {}; // 編集後の判定半サイズ
    for (int iteration = 0; iteration < 100; ++iteration) {
        StageGimmickTransformUtility::CalculateSwitchVolume(basis, { 0, 0, 0 }, { 20, 10, 5 }, center, halfSize);
        Check(halfSize.x == 0 && halfSize.y == 0 && halfSize.z == 0, "Zero scale volume failed");
        StageGimmickTransformUtility::CalculateSwitchVolume(basis, basis.scale, basis.translate, center, halfSize);
        Check(center.x == basis.center.x && center.y == basis.center.y && center.z == basis.center.z
            && halfSize.x == basis.halfSize.x && halfSize.y == basis.halfSize.y && halfSize.z == basis.halfSize.z,
            "Switch volume failed to restore after zero scale");
    }
    StageGimmickTransformUtility::CalculateSwitchVolume(basis, { -4, 2, -8 }, { 5, 6, 7 }, center, halfSize);
    Check(center.x == 5 && center.y == 7 && center.z == 7
        && halfSize.x == 2 && halfSize.y == 4 && halfSize.z == 6, "Edited switch volume mismatch");
}
}

/// <summary>
/// 既存ステージの互換性、型検証、編集状態の独立性を検証する。
/// </summary>
int main()
{
    try {
        std::ifstream file("project/resources/levels/trace_shift_stage.json"); // 読み取り専用の既存ステージ
        Check(file.good(), "Stage fixture not found; run from repository root");
        nlohmann::json original; // 既存の保存形式
        file >> original;
        TraceShiftStageJson::StageData data; // 検証済みの配置データ
        std::string error; // 読込結果の詳細
        Check(TraceShiftStageJson::Decode(original, data, error), "Existing stage failed to load");
        const auto encoded = TraceShiftStageJson::Encode(data); // メモリ上で往復変換したJSON
        Check(encoded == original, "Existing stage round trip changed values or fields");
        Check(TraceShiftStageJson::Decode(nlohmann::json::parse(encoded.dump(4)), data, error), "Serialized stage failed to load");
        TestGimmickDefaults(original);
        TestSerialization(original);
        TestSwitchVolumeRestoration();

        for (const auto& version : { nlohmann::json(0), nlohmann::json(2), nlohmann::json(-1),
            nlohmann::json(1.5), nlohmann::json(true), nlohmann::json("1"), nlohmann::json(uint64_t{ 0x100000000ULL }) }) {
            auto invalid = original; // 型または対応範囲が不正な保存形式
            invalid["schema_version"] = version;
            CheckRejected(invalid);
        }
        auto legacy = original; // バージョン未指定の旧形式
        legacy.erase("schema_version");
        Check(TraceShiftStageJson::Decode(legacy, data, error), "Legacy schema default failed");
        legacy["gimmicks"] = nlohmann::json::array();
        Check(TraceShiftStageJson::Decode(legacy, data, error) && data.gimmicks.empty(), "Empty gimmicks array was rejected");
        auto invalidGimmicks = original; // IDと必須上端を検証する入力
        invalidGimmicks["gimmicks"][0]["id"] = "unknown_switch";
        CheckRejected(invalidGimmicks);
        for (const auto& gimmick : original["gimmicks"]) {
            invalidGimmicks = original;
            invalidGimmicks["gimmicks"].push_back(gimmick);
            CheckRejected(invalidGimmicks);
        }
        invalidGimmicks = original;
        for (auto& gimmick : invalidGimmicks["gimmicks"]) {
            if (gimmick["id"] == "toggle_elevator") {
                gimmick["transform"].erase("upper_translate");
            }
        }
        CheckRejected(invalidGimmicks);
        invalidGimmicks = original;
        invalidGimmicks["gimmicks"][0]["transform"]["upper_translate"] = { 1, 2, 3 };
        CheckRejected(invalidGimmicks);

        auto changed = original; // 各検証で変更する入力JSON
        changed["name"] = 12;
        CheckRejected(changed);
        changed = original;
        changed["blocks"] = nlohmann::json::array();
        CheckRejected(changed);
        changed = original;
        for (auto& block : changed["blocks"]) {
            block["goal_marker"] = false;
        }
        CheckRejected(changed);
        for (auto& block : changed["blocks"]) {
            block.erase("goal_marker");
        }
        CheckRejected(changed);
        changed = original;
        changed["blocks"][0]["goal_marker"] = true;
        changed["blocks"][1]["goal_marker"] = true;
        CheckRejected(changed);
        changed = original;
        for (auto& block : changed["blocks"]) {
            block["goal_marker"] = false;
        }
        changed["blocks"][0]["goal_marker"] = true;
        changed["blocks"][0]["transform"]["translate"] = { 20, 5, 0 };
        Check(TraceShiftStageJson::Decode(changed, data, error), "Single relocated goal failed to load");
        Check(data.blocks[0].goalMarker && data.blocks[0].translate.x == 20, "Relocated goal was not applied");
        for (const char* key : { "collidable", "one_clone_goal_platform", "goal_marker", "name", "route_mask" }) {
            changed = original;
            changed["blocks"][0][key] = nlohmann::json::array();
            CheckRejected(changed);
        }
        for (const auto& value : { nlohmann::json(-1), nlohmann::json(1.5), nlohmann::json(uint64_t{ 0x100000000ULL }) }) {
            changed = original;
            changed["blocks"][0]["route_mask"] = value;
            CheckRejected(changed);
        }
        changed = original;
        changed["blocks"][0]["transform"]["scale"] = { 1.0, 2.0 };
        CheckRejected(changed);
        changed = original;
        changed["blocks"][0]["color"][0] = 1.0e100;
        CheckRejected(changed);
        changed = original;
        changed["blocks"][0]["color"][0] = std::numeric_limits<double>::infinity();
        CheckRejected(changed);
        changed = original;
        changed["gimmicks"] = false;
        CheckRejected(changed);
        changed = original;
        changed["gimmicks"] = { { { "id", 42 }, { "transform", original["blocks"][0]["transform"] } } };
        CheckRejected(changed);
        changed["gimmicks"][0]["id"] = "toggle_elevator";
        changed["gimmicks"][0]["transform"]["upper_translate"] = { 1, "bad", 3 };
        CheckRejected(changed);

        changed = original;
        changed.erase("gimmicks");
        changed["blocks"][0].erase("name");
        changed["blocks"][0].erase("collidable");
        changed["blocks"][0]["route_mask"] = 8;
        Check(TraceShiftStageJson::Decode(changed, data, error), "Legacy optional defaults failed");
        Check(data.gimmicks.empty() && data.blocks[0].collidable
            && data.blocks[0].name == "Stage Block 0" && data.blocks[0].routeMask == 4, "Optional defaults changed");

        TraceShiftStageJson::GimmickLayout elevator; // 上下端を持つ昇降足場の保存情報
        elevator.id = "toggle_elevator";
        elevator.translate = { 1, 2, 3 };
        elevator.upperTranslate = { 4, 5, 6 };
        elevator.hasUpperTranslate = true;
        data.gimmicks.push_back(elevator);
        Check(TraceShiftStageJson::Decode(TraceShiftStageJson::Encode(data), data, error), "Elevator round trip failed");
        Check(data.gimmicks[0].hasUpperTranslate && data.gimmicks[0].translate.y == 2
            && data.gimmicks[0].upperTranslate.y == 5, "Elevator endpoints changed");

        PlaySceneLevelEditorState first; // 最初のシーンの編集状態
        PlaySceneLevelEditorState second; // 別シーンの独立した編集状態
        first.levelUndoHistory.emplace_back();
        first.selectedLevelObjectPaths.push_back("object/0");
        first.pendingLevelAutoSave = true;
        Check(second.levelUndoHistory.empty() && second.selectedLevelObjectPaths.empty()
            && !second.pendingLevelAutoSave, "Editor state leaked between instances");
        first = {};
        Check(first.levelUndoHistory.empty() && !first.pendingLevelAutoSave, "Editor state reset failed");
        std::cout << "TraceShiftStageJson tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
