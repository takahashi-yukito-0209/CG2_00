#pragma once

#include "../../engine/utility/MathTypes.h"
#include "../../externals/nlohmann/json.hpp"
#include <cstdint>
#include <array>
#include <string_view>
#include <string>
#include <vector>

/// <summary>
/// 描画オブジェクトを持たない実ステージの保存データとJSON変換。
/// </summary>
namespace TraceShiftStageJson {
inline constexpr uint32_t kSchemaVersion = 1; // 対応する実ステージ保存形式
inline constexpr std::array<std::string_view, 10> kGimmickIds = { // 配置を受け付けるギミックID
    "clone_switch", "linked_door", "timed_switch", "timed_door", "toggle_switch",
    "toggle_gate", "weight_switch", "goal_bridge", "one_way_gate", "toggle_elevator",
};
inline constexpr uint32_t kFinalRouteMask = 1u << 2; // 未指定時に使用する最終ルート
inline constexpr uint32_t kAllRouteMask = 7u; // 保存形式で定義する全ルート

struct BlockData {
    std::string name; // 編集用のブロック名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // 表示スケール
    Math::Vector3 rotate {}; // 表示回転
    Math::Vector3 translate {}; // 配置座標
    Math::Vector4 color { 1.0f, 1.0f, 1.0f, 1.0f }; // 状態による変色前の通常色
    uint32_t routeMask = kFinalRouteMask; // 使用するルート
    bool collidable = true; // 地形衝突を有効にするか
    bool oneCloneGoalPlatform = false; // 1体ルートの到達床か
    bool goalMarker = false; // ゴール表示か
};

struct GimmickLayout {
    std::string id; // 配置対象を識別するID
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // 表示スケール
    Math::Vector3 rotate {}; // 表示回転
    Math::Vector3 translate {}; // 配置座標または昇降足場の下端
    Math::Vector3 upperTranslate {}; // 昇降足場の上端
    bool hasUpperTranslate = false; // 上端が指定されているか
};

struct StageData {
    std::vector<BlockData> blocks; // 固定ブロックの保存情報
    std::vector<GimmickLayout> gimmicks; // ギミックの配置情報
};

/// <summary>
/// JSON全体を検証し、成功時だけ保存データを置き換える。
/// </summary>
bool Decode(const nlohmann::json& root, StageData& data, std::string& error);

/// <summary>
/// 保存データを既存の実ステージJSON形式へ変換する。
/// </summary>
nlohmann::json Encode(const StageData& data);

/// <summary>
/// 読み込み条件を満たす保存データを文字列へ変換し、失敗時は出力を変更しない。
/// </summary>
bool Serialize(const StageData& data, std::string& text, std::string& error);

/// <summary>
/// 初期配置を基準に検証済みの指定配置を上書きし、省略された配置を初期値に戻す。
/// </summary>
std::vector<GimmickLayout> ResolveGimmickLayouts(const std::vector<GimmickLayout>& defaults, const std::vector<GimmickLayout>& overrides);
}
