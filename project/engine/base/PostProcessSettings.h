#pragma once

#include <MathTypes.h>
#include <cstdint>

namespace MyEngine {

constexpr uint32_t kPostProcessKernelSize3x3 = 3u; // 3x3カーネルサイズ
constexpr uint32_t kPostProcessKernelSize5x5 = 5u; // 5x5カーネルサイズ
constexpr uint32_t kPostProcessKernelSize7x7 = 7u; // 7x7カーネルサイズ
constexpr float kPostProcessNormalizedValueMin = 0.0f; // 正規化値の最小値
constexpr float kPostProcessNormalizedValueMax = 1.0f; // 正規化値の最大値
constexpr float kPostProcessGaussianSigmaMin = 0.1f; // Gaussian sigmaの最小値
constexpr float kPostProcessGaussianSigmaMax = 10.0f; // Gaussian sigmaの最大値
constexpr float kPostProcessOutlineStrengthMin = 0.0f; // アウトライン強度の最小値
constexpr float kPostProcessOutlineStrengthMax = 32.0f; // アウトライン強度の最大値
constexpr float kPostProcessDepthOutlineSoftnessMin = 0.0001f; // 深度アウトライン柔らかさの最小値
constexpr float kPostProcessRadialBlurWidthMax = 0.1f; // ラジアルブラー幅の最大値
constexpr uint32_t kPostProcessRadialBlurSampleMin = 1u; // ラジアルブラーサンプル数の最小値
constexpr uint32_t kPostProcessRadialBlurSampleMax = 32u; // ラジアルブラーサンプル数の最大値
constexpr float kPostProcessDistortionStrengthMin = -0.1f; // 歪み強度の最小値
constexpr float kPostProcessDistortionStrengthMax = 0.1f; // 歪み強度の最大値
constexpr float kPostProcessDistortionRadiusMin = 0.01f; // 歪み半径の最小値
constexpr float kPostProcessDistortionRadiusMax = 1.5f; // 歪み半径の最大値
constexpr float kPostProcessDistortionWaveCountMax = 12.0f; // 歪み波数の最大値
constexpr float kPostProcessDissolveEdgeWidthMin = 0.001f; // Dissolve境界幅の最小値
constexpr float kPostProcessDissolveEdgeWidthMax = 0.25f; // Dissolve境界幅の最大値
constexpr float kPostProcessRandomScaleMin = 1.0f; // ノイズスケールの最小値
constexpr float kPostProcessRandomScaleMax = 2000.0f; // ノイズスケールの最大値
constexpr float kPostProcessRandomSpeedMax = 20.0f; // ノイズ速度の最大値
constexpr float kPostProcessPositiveDeltaTimeMin = 0.0f; // 時間更新を行う最小デルタ時間

/// <summary>
/// ポストエフェクトの数値設定を保持する構造体
/// </summary>
struct PostProcessSettings {
    uint32_t boxFilterKernelSize = kPostProcessKernelSize3x3; // Box Filterに使用するカーネルサイズ
    uint32_t gaussianKernelSize = kPostProcessKernelSize7x7; // Gaussian Filterに使用するカーネルサイズ
    float gaussianSigma = 2.0f; // Gaussian Filterの標準偏差
    float outlineStrength = 6.0f; // Outlineの輪郭強度
    float depthOutlineThreshold = 0.02f; // 輪郭として扱う相対深度差
    float depthOutlineSoftness = 0.03f; // 輪郭の滑らかな立ち上がり幅
    Math::Vector2 radialBlurCenter = { 0.5f, 0.5f }; // 放射状ブラーの中心
    float radialBlurWidth = 0.01f; // 1サンプルごとのUV移動量
    uint32_t radialBlurSampleCount = 10u; // 放射状ブラーのサンプル数
    float dissolveThreshold = 0.0f; // マスクを破棄する閾値
    float dissolveEdgeWidth = 0.03f; // Dissolve境界の幅
    Math::Vector3 dissolveEdgeColor = { 1.0f, 0.4f, 0.3f }; // Dissolve境界の色
    float randomTime = 0.0f; // RandomのSeedに使用する経過時間
    float randomStrength = 1.0f; // 入力画像へ乗算するノイズ強度
    float randomScale = 600.0f; // UVに掛けるノイズの細かさ
    float randomSpeed = 1.0f; // ノイズの時間変化速度
    Math::Vector2 distortionCenter = { 0.5f, 0.5f }; // 画面歪みの中心UV座標
    float distortionStrength = 0.02f; // 画面歪みの強度
    float distortionRadius = 0.35f; // 画面歪みの影響半径
    float distortionWaveCount = 3.0f; // 画面歪みの波数
    float distortionProgress = 0.0f; // 画面歪みの進行率

    /// <summary>
    /// Box Filterのカーネルサイズを設定する
    /// </summary>
    void SetBoxFilterKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Gaussian Filterのカーネルサイズを設定する
    /// </summary>
    void SetGaussianKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Gaussian Filterの標準偏差を設定する
    /// </summary>
    void SetGaussianSigma(float sigma);

    /// <summary>
    /// Outlineの強度を設定する
    /// </summary>
    void SetOutlineStrength(float strength);

    /// <summary>
    /// Depth Outlineの深度差閾値を設定する
    /// </summary>
    void SetDepthOutlineThreshold(float threshold);

    /// <summary>
    /// Depth Outlineの輪郭の立ち上がり幅を設定する
    /// </summary>
    void SetDepthOutlineSoftness(float softness);

    /// <summary>
    /// Radial Blurの中心UV座標を設定する
    /// </summary>
    void SetRadialBlurCenter(const Math::Vector2& center);

    /// <summary>
    /// Radial Blurの幅を設定する
    /// </summary>
    void SetRadialBlurWidth(float width);

    /// <summary>
    /// Radial Blurのサンプル数を設定する
    /// </summary>
    void SetRadialBlurSampleCount(uint32_t sampleCount);

    /// <summary>
    /// 画面歪みの中心UV座標を設定する
    /// </summary>
    void SetDistortionCenter(const Math::Vector2& center);

    /// <summary>
    /// 画面歪みの強度を設定する
    /// </summary>
    void SetDistortionStrength(float strength);

    /// <summary>
    /// 画面歪みの影響半径を設定する
    /// </summary>
    void SetDistortionRadius(float radius);

    /// <summary>
    /// 画面歪みの波数を設定する
    /// </summary>
    void SetDistortionWaveCount(float waveCount);

    /// <summary>
    /// 画面歪みの進行率を設定する
    /// </summary>
    void SetDistortionProgress(float progress);

    /// <summary>
    /// Dissolveの閾値を設定する
    /// </summary>
    void SetDissolveThreshold(float threshold);

    /// <summary>
    /// Dissolve境界の幅を設定する
    /// </summary>
    void SetDissolveEdgeWidth(float edgeWidth);

    /// <summary>
    /// Dissolve境界の色を設定する
    /// </summary>
    void SetDissolveEdgeColor(const Math::Vector3& color);

    /// <summary>
    /// Randomのノイズ強度を設定する
    /// </summary>
    void SetRandomStrength(float strength);

    /// <summary>
    /// Randomのノイズスケールを設定する
    /// </summary>
    void SetRandomScale(float scale);

    /// <summary>
    /// Randomの時間変化速度を設定する
    /// </summary>
    void SetRandomSpeed(float speed);

    /// <summary>
    /// Randomの経過時間を進める
    /// </summary>
    void AddRandomTime(float deltaTime);
};

} // namespace MyEngine