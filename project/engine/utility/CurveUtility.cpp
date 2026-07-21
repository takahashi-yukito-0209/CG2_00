#include "CurveUtility.h"
#include <algorithm>

namespace {

/// <summary>
/// 補間率を 0.0f から 1.0f の範囲に収める。
/// </summary>
float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

namespace CurveUtility {

/// <summary>
/// 2次ベジェ曲線上の位置を計算する。
/// </summary>
Math::Vector3 EvaluateQuadraticBezier(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    float t)
{
    const float rate = Clamp01(t); // 補間率
    const float inverseRate = 1.0f - rate; // 逆方向の補間率

    return p0 * (inverseRate * inverseRate)
        + p1 * (2.0f * inverseRate * rate)
        + p2 * (rate * rate);
}

/// <summary>
/// 3次ベジェ曲線上の位置を計算する。
/// </summary>
Math::Vector3 EvaluateCubicBezier(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    const Math::Vector3& p3,
    float t)
{
    const float rate = Clamp01(t); // 補間率
    const float inverseRate = 1.0f - rate; // 逆方向の補間率
    const float inverseRateSquared = inverseRate * inverseRate; // 逆方向補間率の二乗
    const float rateSquared = rate * rate; // 補間率の二乗

    return p0 * (inverseRateSquared * inverseRate)
        + p1 * (3.0f * inverseRateSquared * rate)
        + p2 * (3.0f * inverseRate * rateSquared)
        + p3 * (rateSquared * rate);
}

/// <summary>
/// 2次ベジェ曲線の接線ベクトルを計算する。
/// </summary>
Math::Vector3 EvaluateQuadraticBezierTangent(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    float t)
{
    const float rate = Clamp01(t); // 補間率
    const float inverseRate = 1.0f - rate; // 逆方向の補間率

    return (p1 - p0) * (2.0f * inverseRate)
        + (p2 - p1) * (2.0f * rate);
}

/// <summary>
/// 3次ベジェ曲線の接線ベクトルを計算する。
/// </summary>
Math::Vector3 EvaluateCubicBezierTangent(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    const Math::Vector3& p3,
    float t)
{
    const float rate = Clamp01(t); // 補間率
    const float inverseRate = 1.0f - rate; // 逆方向の補間率

    return (p1 - p0) * (3.0f * inverseRate * inverseRate)
        + (p2 - p1) * (6.0f * inverseRate * rate)
        + (p3 - p2) * (3.0f * rate * rate);
}

} // namespace CurveUtility