#pragma once

#include <cstddef>

/// <summary>
/// ステージごとに調整可能な分身パズルのルール設定。
/// </summary>
struct StageRuleSettings {
    size_t maxStoredClones = 3; // 同時に保存できる分身の最大数
    float maxRecordTime = 8.0f; // 1回の分身記録で使用できる最大秒数
};
