#pragma once

#include "MathTypes.h"
#include <cstdint>

/// <summary>
/// 乱数生成に関するユーティリティ名前空間。
/// </summary>
namespace RandomUtility {

/// <summary>
/// 乱数生成器のシード値を設定する。
/// </summary>
void SetSeed(uint32_t seed);

/// <summary>
/// 指定範囲内の int 乱数を生成する。
/// </summary>
int RandomInt(int min, int max);

/// <summary>
/// 指定範囲内の float 乱数を生成する。
/// </summary>
float RandomFloat(float min, float max);

/// <summary>
/// 各成分を指定範囲内で生成した Vector3 乱数を返す。
/// </summary>
Math::Vector3 RandomVector3(const Math::Vector3& min, const Math::Vector3& max);

/// <summary>
/// 各成分を指定範囲内で生成した Vector4 乱数を返す。
/// </summary>
Math::Vector4 RandomVector4(const Math::Vector4& min, const Math::Vector4& max);

/// <summary>
/// XY 平面上のランダムな単位方向ベクトルを返す。
/// </summary>
Math::Vector2 RandomDirection2D();

/// <summary>
/// 3D 空間内のランダムな単位方向ベクトルを返す。
/// </summary>
Math::Vector3 RandomDirection3D();

} // namespace RandomUtility