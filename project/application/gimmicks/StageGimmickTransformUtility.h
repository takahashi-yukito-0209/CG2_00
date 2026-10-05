#pragma once

#include "../../engine/utility/MathTypes.h"
#include <cmath>

/// <summary>
/// スイッチの入力判定を表示編集へ同期するための初期設定。
/// </summary>
struct SwitchVolumeBasis {
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // 初期表示スケール
    Math::Vector3 translate {}; // 初期表示座標
    Math::Vector3 center {}; // 初期判定中心
    Math::Vector3 halfSize { 0.5f, 0.5f, 0.5f }; // 初期判定半サイズ
};

/// <summary>
/// ギミックの編集判定を、過去の編集順序に依存せず計算する。
/// </summary>
namespace StageGimmickTransformUtility {
/// <summary>
/// 初期設定から現在のスイッチ判定を求め、ゼロ縮小後の再拡大でも範囲を復元する。
/// </summary>
inline void CalculateSwitchVolume(const SwitchVolumeBasis& basis, const Math::Vector3& scale,
    const Math::Vector3& translate, Math::Vector3& center, Math::Vector3& halfSize)
{
    constexpr float kMinimumScale = 0.0001f; // 除算可能とみなす初期スケールの最小値
    center = {
        basis.center.x + translate.x - basis.translate.x,
        basis.center.y + translate.y - basis.translate.y,
        basis.center.z + translate.z - basis.translate.z,
    };
    halfSize = {
        basis.halfSize.x * (std::fabs(basis.scale.x) > kMinimumScale ? std::fabs(scale.x / basis.scale.x) : 1.0f),
        basis.halfSize.y * (std::fabs(basis.scale.y) > kMinimumScale ? std::fabs(scale.y / basis.scale.y) : 1.0f),
        basis.halfSize.z * (std::fabs(basis.scale.z) > kMinimumScale ? std::fabs(scale.z / basis.scale.z) : 1.0f),
    };
}
}
