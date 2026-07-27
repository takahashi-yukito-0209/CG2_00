#include "PostProcessSettings.h"

#include <algorithm>

using namespace MyEngine;

namespace {
/// <summary>
/// 値を正規化範囲へ丸める。
/// </summary>
float ClampNormalizedValue(float value)
{
    return (std::clamp)(value, kPostProcessNormalizedValueMin, kPostProcessNormalizedValueMax);
}
} // namespace

/// <summary>
/// Box Filterのカーネルサイズを設定する。
/// </summary>
void PostProcessSettings::SetBoxFilterKernelSize(uint32_t kernelSize)
{
    if (kernelSize == kPostProcessKernelSize3x3 || kernelSize == kPostProcessKernelSize5x5) {
        boxFilterKernelSize = kernelSize;
    }
}

/// <summary>
/// Gaussian Filterのカーネルサイズを設定する。
/// </summary>
void PostProcessSettings::SetGaussianKernelSize(uint32_t kernelSize)
{
    if (kernelSize == kPostProcessKernelSize3x3
        || kernelSize == kPostProcessKernelSize5x5
        || kernelSize == kPostProcessKernelSize7x7) {
        gaussianKernelSize = kernelSize;
    }
}

/// <summary>
/// Gaussian Filterの標準偏差を設定する。
/// </summary>
void PostProcessSettings::SetGaussianSigma(float sigma)
{
    if (sigma >= kPostProcessGaussianSigmaMin && sigma <= kPostProcessGaussianSigmaMax) {
        gaussianSigma = sigma;
    }
}

/// <summary>
/// Outlineの強度を設定する。
/// </summary>
void PostProcessSettings::SetOutlineStrength(float strength)
{
    if (strength >= kPostProcessOutlineStrengthMin && strength <= kPostProcessOutlineStrengthMax) {
        outlineStrength = strength;
    }
}

/// <summary>
/// Depth Outlineの深度差閾値を設定する。
/// </summary>
void PostProcessSettings::SetDepthOutlineThreshold(float threshold)
{
    if (threshold >= kPostProcessNormalizedValueMin && threshold <= kPostProcessNormalizedValueMax) {
        depthOutlineThreshold = threshold;
    }
}

/// <summary>
/// Depth Outlineの輪郭の立ち上がり幅を設定する。
/// </summary>
void PostProcessSettings::SetDepthOutlineSoftness(float softness)
{
    if (softness >= kPostProcessDepthOutlineSoftnessMin && softness <= kPostProcessNormalizedValueMax) {
        depthOutlineSoftness = softness;
    }
}

/// <summary>
/// Radial Blurの中心UV座標を設定する。
/// </summary>
void PostProcessSettings::SetRadialBlurCenter(const Math::Vector2& center)
{
    radialBlurCenter.x = ClampNormalizedValue(center.x);
    radialBlurCenter.y = ClampNormalizedValue(center.y);
}

/// <summary>
/// Radial Blurの幅を設定する。
/// </summary>
void PostProcessSettings::SetRadialBlurWidth(float width)
{
    if (width >= kPostProcessNormalizedValueMin && width <= kPostProcessRadialBlurWidthMax) {
        radialBlurWidth = width;
    }
}

/// <summary>
/// Radial Blurのサンプル数を設定する。
/// </summary>
void PostProcessSettings::SetRadialBlurSampleCount(uint32_t sampleCount)
{
    if (sampleCount >= kPostProcessRadialBlurSampleMin && sampleCount <= kPostProcessRadialBlurSampleMax) {
        radialBlurSampleCount = sampleCount;
    }
}

/// <summary>
/// 画面歪みの中心UV座標を設定する。
/// </summary>
void PostProcessSettings::SetDistortionCenter(const Math::Vector2& center)
{
    distortionCenter.x = ClampNormalizedValue(center.x);
    distortionCenter.y = ClampNormalizedValue(center.y);
}

/// <summary>
/// 画面歪みの強度を設定する。
/// </summary>
void PostProcessSettings::SetDistortionStrength(float strength)
{
    distortionStrength = (std::clamp)(strength, kPostProcessDistortionStrengthMin, kPostProcessDistortionStrengthMax);
}

/// <summary>
/// 画面歪みの影響半径を設定する。
/// </summary>
void PostProcessSettings::SetDistortionRadius(float radius)
{
    distortionRadius = (std::clamp)(radius, kPostProcessDistortionRadiusMin, kPostProcessDistortionRadiusMax);
}

/// <summary>
/// 画面歪みの波数を設定する。
/// </summary>
void PostProcessSettings::SetDistortionWaveCount(float waveCount)
{
    distortionWaveCount = (std::clamp)(
        waveCount,
        kPostProcessNormalizedValueMin,
        kPostProcessDistortionWaveCountMax);
}

/// <summary>
/// 画面歪みの進行率を設定する。
/// </summary>
void PostProcessSettings::SetDistortionProgress(float progress)
{
    distortionProgress = ClampNormalizedValue(progress);
}

/// <summary>
/// Dissolveの閾値を設定する。
/// </summary>
void PostProcessSettings::SetDissolveThreshold(float threshold)
{
    dissolveThreshold = ClampNormalizedValue(threshold);
}

/// <summary>
/// Dissolve境界の幅を設定する。
/// </summary>
void PostProcessSettings::SetDissolveEdgeWidth(float edgeWidth)
{
    dissolveEdgeWidth = (std::clamp)(
        edgeWidth,
        kPostProcessDissolveEdgeWidthMin,
        kPostProcessDissolveEdgeWidthMax);
}

/// <summary>
/// Dissolve境界の色を設定する。
/// </summary>
void PostProcessSettings::SetDissolveEdgeColor(const Math::Vector3& color)
{
    dissolveEdgeColor.x = ClampNormalizedValue(color.x);
    dissolveEdgeColor.y = ClampNormalizedValue(color.y);
    dissolveEdgeColor.z = ClampNormalizedValue(color.z);
}

/// <summary>
/// Randomのノイズ強度を設定する。
/// </summary>
void PostProcessSettings::SetRandomStrength(float strength)
{
    randomStrength = ClampNormalizedValue(strength);
}

/// <summary>
/// Randomのノイズスケールを設定する。
/// </summary>
void PostProcessSettings::SetRandomScale(float scale)
{
    randomScale = (std::clamp)(scale, kPostProcessRandomScaleMin, kPostProcessRandomScaleMax);
}

/// <summary>
/// Randomの時間変化速度を設定する。
/// </summary>
void PostProcessSettings::SetRandomSpeed(float speed)
{
    randomSpeed = (std::clamp)(speed, kPostProcessNormalizedValueMin, kPostProcessRandomSpeedMax);
}

/// <summary>
/// Randomの経過時間を進める。
/// </summary>
void PostProcessSettings::AddRandomTime(float deltaTime)
{
    if (deltaTime > kPostProcessPositiveDeltaTimeMin) {
        randomTime += deltaTime * randomSpeed;
    }
}