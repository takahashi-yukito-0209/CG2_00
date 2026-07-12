#pragma once

#include <algorithm>
#include <cstdint>

namespace EffectProgress {
constexpr float kMinimumEffectDuration = 0.0001f; // 演出時間のゼロ除算を防ぐ最小値
constexpr float kEffectTimeStart = 0.0f; // フェーズ開始時の経過時間
constexpr float kEffectProgressMin = 0.0f; // 演出進行率の最小値
constexpr float kEffectProgressMax = 1.0f; // 演出進行率の最大値

/// <summary>
/// 演出時間をゼロ除算しない値へ補正する
/// </summary>
inline float GetSafeEffectDuration(float duration)
{
    const float safeDuration = (std::max)(duration, kMinimumEffectDuration); // 進行率計算に使用する演出時間
    return safeDuration;
}

/// <summary>
/// 経過時間から0から1の演出進行率を計算する
/// </summary>
inline float CalculateEffectProgress(float elapsedTime, float duration)
{
    const float safeDuration = GetSafeEffectDuration(duration); // 進行率計算に使用する演出時間
    const float progress = elapsedTime / safeDuration; // clamp前の演出進行率
    return (std::clamp)(progress, kEffectProgressMin, kEffectProgressMax);
}

/// <summary>
/// 経過時間から1から0へ戻る演出進行率を計算する
/// </summary>
inline float CalculateEffectRemainingProgress(float elapsedTime, float duration)
{
    const float progress = CalculateEffectProgress(elapsedTime, duration); // 現在の演出進行率
    return kEffectProgressMax - progress;
}

/// <summary>
/// 設定値を0以上のパーティクル発生数へ補正する
/// </summary>
inline uint32_t ClampParticleCount(int count)
{
    const int safeCount = (std::max)(count, 0); // 負数を発生数として扱わないための補正値
    return static_cast<uint32_t>(safeCount);
}
} // namespace EffectProgress