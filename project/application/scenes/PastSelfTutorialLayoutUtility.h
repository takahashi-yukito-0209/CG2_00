#pragma once

#include "../../engine/utility/MathTypes.h"
#include <cmath>
#include <span>

/// <summary>
/// 現在のステージ配置からチュートリアルの判定と注視対象を求める。
/// </summary>
namespace PastSelfTutorialLayoutUtility {
/// <summary>
/// ゴール表示の現在スケールから、編集履歴に依存しない判定半サイズを求める。
/// 初期表示スケールは各軸が非ゼロであることを前提とする。
/// </summary>
inline Math::Vector3 CalculateGoalHalfSize(const Math::Vector3& markerScale,
    const Math::Vector3& initialMarkerScale, const Math::Vector3& initialGoalHalfSize)
{
    return {
        initialGoalHalfSize.x * std::fabs(markerScale.x / initialMarkerScale.x),
        initialGoalHalfSize.y * std::fabs(markerScale.y / initialMarkerScale.y),
        initialGoalHalfSize.z * std::fabs(markerScale.z / initialMarkerScale.z),
    };
}

/// <summary>
/// 昇降足場が編集後の上端へ到達しているか、各軸の許容誤差で確認する。
/// </summary>
inline bool IsAtUpperEndpoint(const Math::Vector3& current, const Math::Vector3& upper, float tolerance)
{
    return std::fabs(current.x - upper.x) <= tolerance
        && std::fabs(current.y - upper.y) <= tolerance
        && std::fabs(current.z - upper.z) <= tolerance;
}

/// <summary>
/// プレイヤー全体が扉の右端を通過したか、現在の判定範囲で確認する。
/// </summary>
inline bool HasPassedDoor(const Math::Vector3& playerCenter, const Math::Vector3& playerHalfSize,
    const Math::Vector3& doorCenter, const Math::Vector3& doorHalfSize)
{
    return playerCenter.x - playerHalfSize.x > doorCenter.x + doorHalfSize.x;
}

/// <summary>
/// 既存の攻略順で前方にある配置を選び、通過済みならゴールを注視する。
/// </summary>
inline Math::Vector3 SelectCameraTarget(float playerX, std::span<const Math::Vector3> targets, const Math::Vector3& goalCenter)
{
    for (const Math::Vector3& target : targets) {
        if (playerX < target.x) {
            return target;
        }
    }
    return goalCenter;
}
}
