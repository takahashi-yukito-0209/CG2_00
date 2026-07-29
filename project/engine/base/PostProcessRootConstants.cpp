#include "PostProcessRootConstants.h"

#include "../utility/mathUtility.h"

#include <cstring>

namespace MyEngine::PostProcessRootConstants {
namespace {
/// <summary>
/// float値を32bitルート定数へ渡すビット表現へ変換する。
/// </summary>
uint32_t ConvertFloatToRootConstant(float value)
{
    uint32_t bits = 0; // float値のビット表現
    std::memcpy(&bits, &value, sizeof(float));
    return bits;
}

/// <summary>
/// float値を指定した位置のルート定数へ書き込む。
/// </summary>
void WriteFloatRootConstant(uint32_t* constants, uint32_t index, float value)
{
    constants[index] = ConvertFloatToRootConstant(value);
}
} // namespace

/// <summary>
/// 通常の1passポストエフェクト用ルート定数を作成する。
/// </summary>
RootConstantData BuildTexture(PostEffectType effectType, const PostProcessSettings& settings)
{
    RootConstantData filterSettings {}; // 通常の1passエフェクト用設定
    filterSettings[0] = settings.boxFilterKernelSize;
    filterSettings[2] = ConvertFloatToRootConstant(settings.gaussianSigma);
    WriteFloatRootConstant(filterSettings.data(), 3, settings.outlineStrength);
    std::memcpy(&filterSettings[4], &settings.radialBlurCenter, sizeof(Math::Vector2));
    WriteFloatRootConstant(filterSettings.data(), 6, settings.radialBlurWidth);
    filterSettings[7] = settings.radialBlurSampleCount;
    WriteFloatRootConstant(filterSettings.data(), 8, settings.dissolveThreshold);
    WriteFloatRootConstant(filterSettings.data(), 9, settings.dissolveEdgeWidth);
    std::memcpy(&filterSettings[12], &settings.dissolveEdgeColor, sizeof(Math::Vector3));
    WriteFloatRootConstant(filterSettings.data(), 16, settings.randomTime);
    WriteFloatRootConstant(filterSettings.data(), 17, settings.randomStrength);
    WriteFloatRootConstant(filterSettings.data(), 18, settings.randomScale);

    if (effectType == PostEffectType::Distortion) {
        std::memcpy(&filterSettings[4], &settings.distortionCenter, sizeof(Math::Vector2));
        WriteFloatRootConstant(filterSettings.data(), 6, settings.distortionStrength);
        WriteFloatRootConstant(filterSettings.data(), 8, settings.distortionRadius);
        WriteFloatRootConstant(filterSettings.data(), 9, settings.distortionWaveCount);
        WriteFloatRootConstant(filterSettings.data(), 10, settings.distortionProgress);
    }

    return filterSettings;
}

/// <summary>
/// Gaussian Filter用ルート定数を作成する。
/// </summary>
RootConstantData BuildGaussian(uint32_t direction, const PostProcessSettings& settings)
{
    RootConstantData filterSettings {}; // Gaussian Filter用設定
    filterSettings[0] = settings.gaussianKernelSize;
    filterSettings[1] = direction;
    filterSettings[2] = ConvertFloatToRootConstant(settings.gaussianSigma);
    return filterSettings;
}

/// <summary>
/// Dissolve用ルート定数を作成する。
/// </summary>
RootConstantData BuildDissolve(const PostProcessSettings& settings)
{
    RootConstantData dissolveSettings {}; // Dissolve用ルート定数
    WriteFloatRootConstant(dissolveSettings.data(), 8, settings.dissolveThreshold);
    WriteFloatRootConstant(dissolveSettings.data(), 9, settings.dissolveEdgeWidth);
    std::memcpy(&dissolveSettings[12], &settings.dissolveEdgeColor, sizeof(Math::Vector3));
    return dissolveSettings;
}

/// <summary>
/// Depth Outline用ルート定数を作成する。
/// </summary>
RootConstantData BuildDepthOutline(
    const Math::Matrix4x4& projectionMatrix,
    const PostProcessSettings& settings)
{
    RootConstantData outlineSettings {}; // Outline用ルート定数
    Math::Matrix4x4 projectionInverse = MathUtil::Inverse(projectionMatrix); // View空間復元用の逆射影行列
    WriteFloatRootConstant(outlineSettings.data(), 3, settings.outlineStrength);
    WriteFloatRootConstant(outlineSettings.data(), 0, settings.depthOutlineThreshold);
    WriteFloatRootConstant(outlineSettings.data(), 1, settings.depthOutlineSoftness);
    std::memcpy(&outlineSettings[4], &projectionInverse, sizeof(Math::Matrix4x4));
    return outlineSettings;
}

} // namespace MyEngine::PostProcessRootConstants