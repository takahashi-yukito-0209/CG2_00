#include "Easing.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

/// <summary>
/// 補間率を 0.0f から 1.0f の範囲に収める。
/// </summary>
float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

namespace Easing {

float Linear(float t)
{
    return Clamp01(t);
}

float InSine(float t)
{
    const float rate = Clamp01(t); // 補間率
    return 1.0f - std::cos((rate * std::numbers::pi_v<float>) * 0.5f);
}

float OutSine(float t)
{
    const float rate = Clamp01(t); // 補間率
    return std::sin((rate * std::numbers::pi_v<float>) * 0.5f);
}

float InOutSine(float t)
{
    const float rate = Clamp01(t); // 補間率
    return -(std::cos(std::numbers::pi_v<float> * rate) - 1.0f) * 0.5f;
}

float InQuad(float t)
{
    const float rate = Clamp01(t); // 補間率
    return rate * rate;
}

float OutQuad(float t)
{
    const float rate = Clamp01(t); // 補間率
    return 1.0f - (1.0f - rate) * (1.0f - rate);
}

float InOutQuad(float t)
{
    const float rate = Clamp01(t); // 補間率
    if (rate < 0.5f) {
        return 2.0f * rate * rate;
    }

    const float reverseRate = -2.0f * rate + 2.0f; // 終端側の残り率
    return 1.0f - (reverseRate * reverseRate) * 0.5f;
}

float InCubic(float t)
{
    const float rate = Clamp01(t); // 補間率
    return rate * rate * rate;
}

float OutCubic(float t)
{
    const float rate = Clamp01(t); // 補間率
    const float reverseRate = 1.0f - rate; // 終端側の残り率
    return 1.0f - reverseRate * reverseRate * reverseRate;
}

float InOutCubic(float t)
{
    const float rate = Clamp01(t); // 補間率
    if (rate < 0.5f) {
        return 4.0f * rate * rate * rate;
    }

    const float reverseRate = -2.0f * rate + 2.0f; // 終端側の残り率
    return 1.0f - (reverseRate * reverseRate * reverseRate) * 0.5f;
}

float InBack(float t)
{
    constexpr float kBackStrength = 1.70158f; // 戻りの強さ
    const float rate = Clamp01(t); // 補間率
    return (kBackStrength + 1.0f) * rate * rate * rate - kBackStrength * rate * rate;
}

float OutBack(float t)
{
    constexpr float kBackStrength = 1.70158f; // 戻りの強さ
    const float rate = Clamp01(t) - 1.0f; // 終端基準の補間率
    return 1.0f + (kBackStrength + 1.0f) * rate * rate * rate + kBackStrength * rate * rate;
}

float InOutBack(float t)
{
    constexpr float kBackStrength = 1.70158f * 1.525f; // 戻りの強さ
    const float rate = Clamp01(t) * 2.0f; // 前半と後半に分ける補間率
    if (rate < 1.0f) {
        return (rate * rate * ((kBackStrength + 1.0f) * rate - kBackStrength)) * 0.5f;
    }

    const float shiftedRate = rate - 2.0f; // 終端基準の補間率
    return (shiftedRate * shiftedRate * ((kBackStrength + 1.0f) * shiftedRate + kBackStrength) + 2.0f) * 0.5f;
}

float OutBounce(float t)
{
    constexpr float kBounceScale = 7.5625f; // 跳ね返りの倍率
    constexpr float kBounceStep = 2.75f; // 跳ね返り区間の分割値
    float rate = Clamp01(t); // 補間率

    if (rate < 1.0f / kBounceStep) {
        return kBounceScale * rate * rate;
    }
    if (rate < 2.0f / kBounceStep) {
        rate -= 1.5f / kBounceStep;
        return kBounceScale * rate * rate + 0.75f;
    }
    if (rate < 2.5f / kBounceStep) {
        rate -= 2.25f / kBounceStep;
        return kBounceScale * rate * rate + 0.9375f;
    }

    rate -= 2.625f / kBounceStep;
    return kBounceScale * rate * rate + 0.984375f;
}

float InBounce(float t)
{
    const float rate = Clamp01(t); // 補間率
    return 1.0f - OutBounce(1.0f - rate);
}

float InOutBounce(float t)
{
    const float rate = Clamp01(t); // 補間率
    if (rate < 0.5f) {
        return (1.0f - OutBounce(1.0f - 2.0f * rate)) * 0.5f;
    }

    return (1.0f + OutBounce(2.0f * rate - 1.0f)) * 0.5f;
}

} // namespace Easing