#pragma once

#include <MathTypes.h>
#include "PostProcessSettings.h"
#include <cstdint>
#include <memory>

namespace MyEngine {

class DirectXCommon;
class PostProcessPipeline;

/// <summary>
/// ポストエフェクトの種類
/// </summary>
enum class PostEffectType {
    Distortion, // 指定した中心から画面を円形に歪ませる
    Copy, // 元画像をそのまま描画する
    Grayscale, // 元画像をグレースケール化して描画する
    Vignette, // 画面周辺を暗くして描画する
    BoxFilter, // Box Filterによる平滑化処理を適用する
    GaussianFilter, // 分離型Gaussian Filterを適用する
    LuminanceOutline, // 輝度差から輪郭を検出する
    DepthOutline, // 深度差から輪郭を検出する
    RadialBlur, // 指定した中心から放射状にぼかす
    Dissolve, // ノイズマスクの閾値で画面を消去する
    Random, // GPUで生成した乱数を入力画像へ乗算する
    Count, // ポストエフェクト種類数
};

/// <summary>
/// ポストエフェクト種類数を取得する。
/// </summary>
constexpr uint32_t GetPostEffectTypeCount()
{
    return static_cast<uint32_t>(PostEffectType::Count);
}

/// <summary>
/// ポストエフェクト種類の最大番号を取得する。
/// </summary>
constexpr uint32_t GetMaxPostEffectTypeIndex()
{
    return GetPostEffectTypeCount() - 1u;
}

/// <summary>
/// ポストエフェクト種類が有効範囲内か確認する。
/// </summary>
constexpr bool IsValidPostEffectType(PostEffectType effectType)
{
    return static_cast<uint32_t>(effectType) < GetPostEffectTypeCount();
}

/// <summary>
/// ポストエフェクト種類の表示名を取得する。
/// </summary>
const char* GetPostEffectTypeName(PostEffectType effectType);
/// <summary>
/// 全画面ポストエフェクトを管理するクラス
/// </summary>
class PostProcess {
public:
    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    PostProcess();

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~PostProcess();

    /// <summary>
    /// ポストエフェクトに必要なリソースを初期化する
    /// </summary>
    bool Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// ポストエフェクトが保持するリソースを解放する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 指定したポストエフェクトでテクスチャを描画する
    /// </summary>
    void DrawTexture(uint32_t srvIndex, PostEffectType effectType);

    /// <summary>
    /// 現在選択されているポストエフェクトでテクスチャを描画する
    /// </summary>
    void DrawTexture(uint32_t srvIndex);

    /// <summary>
    /// 描画に必要なリソースが揃っているか確認する
    /// </summary>
    bool IsReady() const;

    /// <summary>
    /// ポストエフェクトの有効状態を設定する
    /// </summary>
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    /// <summary>
    /// ポストエフェクトが有効か確認する
    /// </summary>
    bool IsEnabled() const { return enabled_; }

    /// <summary>
    /// 使用するポストエフェクトを設定する
    /// </summary>
    void SetEffectType(PostEffectType effectType)
    {
        effectType_ = IsValidPostEffectType(effectType) ? effectType : PostEffectType::Copy;
    }

    /// <summary>
    /// 現在選択されているポストエフェクトを取得する
    /// </summary>
    PostEffectType GetEffectType() const { return effectType_; }

    /// <summary>
    /// 通常コピー用PSOが生成済みか確認する
    /// </summary>
    bool IsCopyReady() const;

    /// <summary>
    /// グレースケール用PSOが生成済みか確認する
    /// </summary>
    bool IsGrayscaleReady() const;

    /// <summary>
    /// ビネット用PSOが生成済みか確認する
    /// </summary>
    bool IsVignetteReady() const;

    /// <summary>
    /// Box Filter用PSOが生成済みか確認する
    /// </summary>
    bool IsBoxFilterReady() const;

    /// <summary>
    /// Box Filterで使用するカーネルサイズを設定する
    /// </summary>
    void SetBoxFilterKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Box Filterで使用するカーネルサイズを取得する
    /// </summary>
    uint32_t GetBoxFilterKernelSize() const { return settings_.boxFilterKernelSize; }

    /// <summary>
    /// Gaussian Filter用PSOが生成済みか確認する
    /// </summary>
    bool IsGaussianFilterReady() const;

    /// <summary>
    /// Gaussian Filterで使用するカーネルサイズを設定する
    /// </summary>
    void SetGaussianKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Gaussian Filterで使用するカーネルサイズを取得する
    /// </summary>
    uint32_t GetGaussianKernelSize() const { return settings_.gaussianKernelSize; }

    /// <summary>
    /// Gaussian Filterで使用する標準偏差を設定する
    /// </summary>
    void SetGaussianSigma(float sigma);

    /// <summary>
    /// Gaussian Filterで使用する標準偏差を取得する
    /// </summary>
    float GetGaussianSigma() const { return settings_.gaussianSigma; }

    /// <summary>
    /// Gaussian Filterの1方向分を現在の描画先へ描画する
    /// </summary>
    void DrawGaussianPass(uint32_t srvIndex, uint32_t direction);

    /// <summary>
    /// Outlineの強度を設定する
    /// </summary>
    void SetOutlineStrength(float strength);

    /// <summary>
    /// Outlineの強度を取得する
    /// </summary>
    float GetOutlineStrength() const { return settings_.outlineStrength; }

    /// <summary>
    /// Depth Outlineで輪郭と判定する深度差の閾値を設定する
    /// </summary>
    void SetDepthOutlineThreshold(float threshold);

    /// <summary>
    /// Depth Outlineの深度差閾値を取得する
    /// </summary>
    float GetDepthOutlineThreshold() const { return settings_.depthOutlineThreshold; }

    /// <summary>
    /// Depth Outlineの輪郭の立ち上がり幅を設定する
    /// </summary>
    void SetDepthOutlineSoftness(float softness);

    /// <summary>
    /// Depth Outlineの輪郭の立ち上がり幅を取得する
    /// </summary>
    float GetDepthOutlineSoftness() const { return settings_.depthOutlineSoftness; }

    /// <summary>
    /// 輝度ベースOutline用PSOが生成済みか確認する
    /// </summary>
    bool IsLuminanceOutlineReady() const;

    /// <summary>
    /// 深度ベースOutline用PSOが生成済みか確認する
    /// </summary>
    bool IsDepthOutlineReady() const;

    /// <summary>
    /// 深度テクスチャを使用してOutlineを描画する
    /// </summary>
    void DrawDepthOutline(
        uint32_t colorSrvIndex,
        uint32_t depthSrvIndex,
        const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// Radial Blur用PSOが生成済みか確認する
    /// </summary>
    bool IsRadialBlurReady() const;

    /// <summary>
    /// Radial Blurの中心UV座標を設定する
    /// </summary>
    void SetRadialBlurCenter(const Math::Vector2& center);

    /// <summary>
    /// Radial Blurの中心UV座標を取得する
    /// </summary>
    const Math::Vector2& GetRadialBlurCenter() const { return settings_.radialBlurCenter; }

    /// <summary>
    /// Radial Blurの幅を設定する
    /// </summary>
    void SetRadialBlurWidth(float width);

    /// <summary>
    /// Radial Blurの幅を取得する
    /// </summary>
    float GetRadialBlurWidth() const { return settings_.radialBlurWidth; }

    /// <summary>
    /// Radial Blurのサンプル数を設定する
    /// </summary>
    void SetRadialBlurSampleCount(uint32_t sampleCount);

    /// <summary>
    /// Radial Blurのサンプル数を取得する
    /// </summary>
    uint32_t GetRadialBlurSampleCount() const { return settings_.radialBlurSampleCount; }

    /// <summary>
    /// 画面歪み用PSOが生成済みか確認する
    /// </summary>
    bool IsDistortionReady() const;

    /// <summary>
    /// 画面歪みの中心UV座標を設定する
    /// </summary>
    void SetDistortionCenter(const Math::Vector2& center);

    /// <summary>
    /// 画面歪みの中心UV座標を取得する
    /// </summary>
    const Math::Vector2& GetDistortionCenter() const { return settings_.distortionCenter; }

    /// <summary>
    /// 画面歪みの強度を設定する
    /// </summary>
    void SetDistortionStrength(float strength);

    /// <summary>
    /// 画面歪みの強度を取得する
    /// </summary>
    float GetDistortionStrength() const { return settings_.distortionStrength; }

    /// <summary>
    /// 画面歪みの半径を設定する
    /// </summary>
    void SetDistortionRadius(float radius);

    /// <summary>
    /// 画面歪みの半径を取得する
    /// </summary>
    float GetDistortionRadius() const { return settings_.distortionRadius; }

    /// <summary>
    /// 画面歪みの波数を設定する
    /// </summary>
    void SetDistortionWaveCount(float waveCount);

    /// <summary>
    /// 画面歪みの波数を取得する
    /// </summary>
    float GetDistortionWaveCount() const { return settings_.distortionWaveCount; }

    /// <summary>
    /// 画面歪みの進行率を設定する
    /// </summary>
    void SetDistortionProgress(float progress);

    /// <summary>
    /// 画面歪みの進行率を取得する
    /// </summary>
    float GetDistortionProgress() const { return settings_.distortionProgress; }

    /// <summary>
    /// Dissolve用PSOが生成済みか確認する
    /// </summary>
    bool IsDissolveReady() const;

    /// <summary>
    /// Dissolveの閾値を設定する
    /// </summary>
    void SetDissolveThreshold(float threshold);

    /// <summary>
    /// Dissolveの閾値を取得する
    /// </summary>
    float GetDissolveThreshold() const { return settings_.dissolveThreshold; }

    /// <summary>
    /// Dissolve境界の幅を設定する
    /// </summary>
    void SetDissolveEdgeWidth(float edgeWidth);

    /// <summary>
    /// Dissolve境界の幅を取得する
    /// </summary>
    float GetDissolveEdgeWidth() const { return settings_.dissolveEdgeWidth; }

    /// <summary>
    /// Dissolve境界の色を設定する
    /// </summary>
    void SetDissolveEdgeColor(const Math::Vector3& color);

    /// <summary>
    /// Dissolve境界の色を取得する
    /// </summary>
    const Math::Vector3& GetDissolveEdgeColor() const { return settings_.dissolveEdgeColor; }

    /// <summary>
    /// ノイズマスクを使用してDissolveを描画する
    /// </summary>
    void DrawDissolveTexture(
        uint32_t sourceSrvIndex,
        uint32_t maskSrvIndex);

    /// <summary>
    /// 時間経過を使用するポストエフェクトを更新する
    /// </summary>
    void Update(float deltaTime);

    /// <summary>
    /// Random用PSOが生成済みか確認する
    /// </summary>
    bool IsRandomReady() const;

    /// <summary>
    /// Randomのノイズ強度を設定する
    /// </summary>
    void SetRandomStrength(float strength);

    /// <summary>
    /// Randomのノイズ強度を取得する
    /// </summary>
    float GetRandomStrength() const { return settings_.randomStrength; }

    /// <summary>
    /// Randomのノイズスケールを設定する
    /// </summary>
    void SetRandomScale(float scale);

    /// <summary>
    /// Randomのノイズスケールを取得する
    /// </summary>
    float GetRandomScale() const { return settings_.randomScale; }

    /// <summary>
    /// Randomの時間変化速度を設定する
    /// </summary>
    void SetRandomSpeed(float speed);

    /// <summary>
    /// Randomの時間変化速度を取得する
    /// </summary>
    float GetRandomSpeed() const { return settings_.randomSpeed; }

    /// <summary>
    /// 最後に描画へ使用したSRVインデックスを取得する
    /// </summary>
    uint32_t GetLastSrvIndex() const { return lastSrvIndex_; }

private:
    DirectXCommon* dxCommon_ = nullptr; // DirectX共通基盤
    std::unique_ptr<PostProcessPipeline> pipeline_; // RootSignatureとPSOの管理先
    PostEffectType effectType_ = PostEffectType::Grayscale; // 現在選択中のエフェクト
    bool enabled_ = true; // ポストエフェクトの有効状態
    uint32_t lastSrvIndex_ = UINT32_MAX; // 最後に描画へ使用したSRVインデックス
    PostProcessSettings settings_; // ポストエフェクトの数値設定
};

} // namespace MyEngine