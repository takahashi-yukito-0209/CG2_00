#pragma once

#include "MathTypes.h"

/// <summary>
/// 曲線評価に関するユーティリティ関数をまとめた名前空間。
/// </summary>
namespace CurveUtility {

/// <summary>
/// 2次ベジェ曲線上の位置を計算する。
/// </summary>
Math::Vector3 EvaluateQuadraticBezier(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    float t);

/// <summary>
/// 3次ベジェ曲線上の位置を計算する。
/// </summary>
Math::Vector3 EvaluateCubicBezier(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    const Math::Vector3& p3,
    float t);

/// <summary>
/// 2次ベジェ曲線の接線ベクトルを計算する。
/// </summary>
Math::Vector3 EvaluateQuadraticBezierTangent(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    float t);

/// <summary>
/// 3次ベジェ曲線の接線ベクトルを計算する。
/// </summary>
Math::Vector3 EvaluateCubicBezierTangent(
    const Math::Vector3& p0,
    const Math::Vector3& p1,
    const Math::Vector3& p2,
    const Math::Vector3& p3,
    float t);

} // namespace CurveUtility