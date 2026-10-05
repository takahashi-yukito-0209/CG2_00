#include "TraceShiftStageJson.h"
#include "../../engine/utility/JsonUtility.h"
#include <utility>
#include <algorithm>

namespace {
/// <summary>
/// 任意のbool項目を読み取り、未指定なら既定値を維持する。
/// </summary>
bool ReadOptionalBool(const nlohmann::json& object, const char* key, bool& value)
{
    const auto item = object.find(key); // 指定された項目
    if (item == object.end()) {
        return true;
    }
    if (!item->is_boolean()) {
        return false;
    }
    value = item->get<bool>();
    return true;
}

/// <summary>
/// Transformの必須項目を読み取る。
/// </summary>
bool ReadTransform(const nlohmann::json& object, Math::Vector3& scale, Math::Vector3& rotate, Math::Vector3& translate)
{
    const auto transform = object.find("transform"); // 配置情報のJSON
    return transform != object.end() && transform->is_object()
        && JsonUtility::ExtractVector3(*transform, "scale", scale)
        && JsonUtility::ExtractVector3(*transform, "rotate", rotate)
        && JsonUtility::ExtractVector3(*transform, "translate", translate);
}

/// <summary>
/// Transformを既存の配列形式で出力する。
/// </summary>
nlohmann::json WriteTransform(const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& translate)
{
    return {
        { "scale", { scale.x, scale.y, scale.z } },
        { "rotate", { rotate.x, rotate.y, rotate.z } },
        { "translate", { translate.x, translate.y, translate.z } },
    };
}
}

/// <summary>
/// JSON全体を検証し、成功時だけ保存データを置き換える。
/// </summary>
bool TraceShiftStageJson::Decode(const nlohmann::json& root, StageData& data, std::string& error)
{
    error.clear();
    std::string name; // 保存形式の識別名
    if (!root.is_object() || !JsonUtility::ExtractString(root, "name", name)
        || name != "trace_shift_stage" || !root.contains("blocks") || !root["blocks"].is_array()) {
        error = "Invalid trace_shift_stage JSON root.";
        return false;
    }
    uint32_t schemaVersion = kSchemaVersion; // 未指定の旧形式はバージョン1として扱う
    if ((root.contains("schema_version") && !JsonUtility::ExtractUint(root, "schema_version", schemaVersion))
        || schemaVersion != kSchemaVersion) {
        error = "Unsupported or invalid stage schema_version.";
        return false;
    }
    StageData loaded; // 全項目の検証が完了するまで保持する仮データ
    for (size_t index = 0; index < root["blocks"].size(); ++index) {
        const auto& object = root["blocks"][index]; // 検証中のブロック
        BlockData block; // 検証後に追加する保存情報
        block.name = "Stage Block " + std::to_string(index);
        if (!object.is_object() || !ReadTransform(object, block.scale, block.rotate, block.translate)
            || !JsonUtility::ExtractVector4(object, "color", block.color)
            || (object.contains("name") && !JsonUtility::ExtractString(object, "name", block.name))
            || !ReadOptionalBool(object, "collidable", block.collidable)
            || !ReadOptionalBool(object, "one_clone_goal_platform", block.oneCloneGoalPlatform)
            || !ReadOptionalBool(object, "goal_marker", block.goalMarker)
            || (object.contains("route_mask") && !JsonUtility::ExtractUint(object, "route_mask", block.routeMask))) {
            error = "Invalid block values at index " + std::to_string(index) + ".";
            return false;
        }
        block.routeMask &= kAllRouteMask;
        if (block.routeMask == 0) {
            block.routeMask = kFinalRouteMask;
        }
        loaded.blocks.push_back(std::move(block));
    }
    if (loaded.blocks.empty()) {
        error = "Stage JSON contains no blocks.";
        return false;
    }
    size_t goalCount = 0; // ゴール表示に指定されたブロック数
    for (const BlockData& block : loaded.blocks) {
        if (block.goalMarker) {
            ++goalCount;
        }
    }
    if (goalCount != 1) {
        error = "Stage JSON must contain exactly one goal marker.";
        return false;
    }
    if (root.contains("gimmicks")) {
        if (!root["gimmicks"].is_array()) {
            error = "Invalid gimmicks array.";
            return false;
        }
        for (size_t index = 0; index < root["gimmicks"].size(); ++index) {
            const auto& object = root["gimmicks"][index]; // 検証中のギミック
            GimmickLayout layout; // 検証後に追加する配置情報
            if (!object.is_object() || !JsonUtility::ExtractString(object, "id", layout.id) || layout.id.empty()
                || !ReadTransform(object, layout.scale, layout.rotate, layout.translate)) {
                error = "Invalid gimmick values at index " + std::to_string(index) + ".";
                return false;
            }
            const auto& transform = object["transform"]; // 必須項目の検証済みTransform
            if (transform.contains("upper_translate")) {
                if (!JsonUtility::ExtractVector3(transform, "upper_translate", layout.upperTranslate)) {
                    error = "Invalid gimmick upper translate at index " + std::to_string(index) + ".";
                    return false;
                }
                layout.hasUpperTranslate = true;
            }
            if (std::find(kGimmickIds.begin(), kGimmickIds.end(), layout.id) == kGimmickIds.end()) {
                error = "Unknown gimmick id: " + layout.id;
                return false;
            }
            if (std::any_of(loaded.gimmicks.begin(), loaded.gimmicks.end(), [&layout](const GimmickLayout& existing) {
                return existing.id == layout.id;
            })) {
                error = "Duplicate gimmick id: " + layout.id;
                return false;
            }
            if ((layout.id == "toggle_elevator") != layout.hasUpperTranslate) {
                error = "upper_translate is required only for toggle_elevator.";
                return false;
            }
            loaded.gimmicks.push_back(std::move(layout));
        }
    }
    data = std::move(loaded);
    return true;
}

/// <summary>
/// 保存データを既存の実ステージJSON形式へ変換する。
/// </summary>
nlohmann::json TraceShiftStageJson::Encode(const StageData& data)
{
    nlohmann::json root = { { "schema_version", kSchemaVersion }, { "name", "trace_shift_stage" },
        { "blocks", nlohmann::json::array() }, { "gimmicks", nlohmann::json::array() } }; // 保存形式のルート
    for (const BlockData& block : data.blocks) {
        root["blocks"].push_back({
            { "name", block.name }, { "transform", WriteTransform(block.scale, block.rotate, block.translate) },
            { "color", { block.color.x, block.color.y, block.color.z, block.color.w } },
            { "collidable", block.collidable }, { "one_clone_goal_platform", block.oneCloneGoalPlatform },
            { "goal_marker", block.goalMarker }, { "route_mask", block.routeMask },
        });
    }
    for (const GimmickLayout& layout : data.gimmicks) {
        auto transform = WriteTransform(layout.scale, layout.rotate, layout.translate); // 出力するギミック配置
        if (layout.hasUpperTranslate) {
            transform["upper_translate"] = { layout.upperTranslate.x, layout.upperTranslate.y, layout.upperTranslate.z };
        }
        root["gimmicks"].push_back({ { "id", layout.id }, { "transform", std::move(transform) } });
    }
    return root;
}

/// <summary>
/// 読み込み条件を満たす保存データを文字列へ変換し、失敗時は出力を変更しない。
/// </summary>
bool TraceShiftStageJson::Serialize(const StageData& data, std::string& text, std::string& error)
{
    error.clear();
    try {
        const nlohmann::json root = Encode(data); // 保存用スナップショットのJSON表現
        StageData validated; // 読み込み時と同じ条件で検証する一時データ
        if (!Decode(root, validated, error)) {
            return false;
        }
        std::string serialized = root.dump(4); // UTF-8文字列の検証も含む保存テキスト
        text = std::move(serialized);
        return true;
    } catch (const nlohmann::json::exception& exception) {
        error = exception.what();
        return false;
    }
}

/// <summary>
/// 初期配置を基準に検証済みの指定配置を上書きし、省略された配置を初期値に戻す。
/// </summary>
std::vector<TraceShiftStageJson::GimmickLayout> TraceShiftStageJson::ResolveGimmickLayouts(
    const std::vector<GimmickLayout>& defaults, const std::vector<GimmickLayout>& overrides)
{
    std::vector<GimmickLayout> layouts = defaults; // 過去の編集状態に依存しない配置一覧
    for (const GimmickLayout& layout : overrides) {
        const auto target = std::find_if(layouts.begin(), layouts.end(), [&layout](const GimmickLayout& initial) {
            return initial.id == layout.id;
        }); // 指定配置に対応する初期配置
        if (target != layouts.end()) {
            *target = layout;
        }
    }
    return layouts;
}
