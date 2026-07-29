#include "RandomUtility.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace {

/// <summary>
/// 共通で使用する乱数生成器を取得する。
/// </summary>
std::mt19937& GetGenerator()
{
    static std::mt19937 generator { std::random_device {}() }; // 共有乱数生成器
    return generator;
}

} // namespace

namespace RandomUtility {

/// <summary>
/// 乱数生成器のシード値を設定する。
/// </summary>
void SetSeed(uint32_t seed)
{
    GetGenerator().seed(seed);
}

/// <summary>
/// 指定範囲内の int 乱数を生成する。
/// </summary>
int RandomInt(int min, int max)
{
    if (min > max) {
        std::swap(min, max);
    }

    std::uniform_int_distribution<int> distribution(min, max); // 整数乱数の分布
    return distribution(GetGenerator());
}

/// <summary>
/// 指定範囲内の float 乱数を生成する。
/// </summary>
float RandomFloat(float min, float max)
{
    if (min > max) {
        std::swap(min, max);
    }

    std::uniform_real_distribution<float> distribution(min, max); // 小数乱数の分布
    return distribution(GetGenerator());
}

/// <summary>
/// 各成分を指定範囲内で生成した Vector3 乱数を返す。
/// </summary>
Math::Vector3 RandomVector3(const Math::Vector3& min, const Math::Vector3& max)
{
    return {
        RandomFloat(min.x, max.x),
        RandomFloat(min.y, max.y),
        RandomFloat(min.z, max.z)
    };
}

/// <summary>
/// 各成分を指定範囲内で生成した Vector4 乱数を返す。
/// </summary>
Math::Vector4 RandomVector4(const Math::Vector4& min, const Math::Vector4& max)
{
    return {
        RandomFloat(min.x, max.x),
        RandomFloat(min.y, max.y),
        RandomFloat(min.z, max.z),
        RandomFloat(min.w, max.w)
    };
}

/// <summary>
/// XY 平面上のランダムな単位方向ベクトルを返す。
/// </summary>
Math::Vector2 RandomDirection2D()
{
    const float angle = RandomFloat(-std::numbers::pi_v<float>, std::numbers::pi_v<float>); // 方向角度
    return { std::cos(angle), std::sin(angle) };
}

/// <summary>
/// 3D 空間内のランダムな単位方向ベクトルを返す。
/// </summary>
Math::Vector3 RandomDirection3D()
{
    for (int retryCount = 0; retryCount < 16; ++retryCount) {
        Math::Vector3 direction = RandomVector3({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }); // 正規化前の方向候補
        float lengthSquared = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z; // 候補の長さの二乗

        if (lengthSquared > 1e-6f && lengthSquared <= 1.0f) {
            float inverseLength = 1.0f / std::sqrt(lengthSquared); // 正規化用の逆長
            return {
                direction.x * inverseLength,
                direction.y * inverseLength,
                direction.z * inverseLength
            };
        }
    }

    return { 1.0f, 0.0f, 0.0f };
}

} // namespace RandomUtility