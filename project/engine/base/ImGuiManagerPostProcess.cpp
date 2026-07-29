#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "engine/base/PostProcess.h"
#include <cstdint>

namespace MyEngine {
namespace {
constexpr int kKernelIndex3x3 = 0; // 3x3を選択する番号
constexpr int kKernelIndex5x5 = 1; // 5x5を選択する番号
constexpr int kKernelIndex7x7 = 2; // 7x7を選択する番号
constexpr uint32_t kSeparablePassCount = 2u; // 分離フィルタのパス数
constexpr float kGaussianSigmaStep = 0.05f; // Gaussian sigmaの調整幅
constexpr float kOutlineStrengthStep = 0.1f; // アウトライン強度の調整幅
constexpr float kDepthThresholdStep = 0.001f; // 深度閾値の調整幅
constexpr float kDepthThresholdMax = 0.2f; // 深度閾値の最大値
constexpr float kDepthSoftnessStep = 0.001f; // 深度アウトライン柔らかさの調整幅
constexpr float kDepthSoftnessMin = 0.001f; // 深度アウトライン柔らかさの最小値
constexpr float kDepthSoftnessMax = 0.2f; // 深度アウトライン柔らかさの最大値
constexpr float kEffectCenterStep = 0.005f; // 中心座標の調整幅
constexpr float kRadialBlurWidthStep = 0.0005f; // ラジアルブラー幅の調整幅
constexpr float kDistortionStrengthStep = 0.001f; // 歪み強度の調整幅
constexpr float kDistortionRadiusStep = 0.01f; // 歪み半径の調整幅
constexpr float kDistortionWaveStep = 0.1f; // 歪み波数の調整幅
constexpr float kDissolveEdgeWidthStep = 0.001f; // Dissolve境界幅の調整幅
constexpr float kRandomScaleStep = 1.0f; // ノイズスケールの調整幅
constexpr float kRandomSpeedStep = 0.05f; // ノイズ速度の調整幅
} // namespace

/// <summary>
/// ポストエフェクトの設定と状態をImGuiへ表示する
/// </summary>
void ImGuiManager::DrawPostProcessSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.postProcess) {
        return;
    }

    if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool enabled = ctx.postProcess->IsEnabled(); // 現在の有効状態
        if (ImGui::Checkbox("Enabled", &enabled)) {
            ctx.postProcess->SetEnabled(enabled);
        }

        int effectIndex = static_cast<int>(ctx.postProcess->GetEffectType()); // 現在のエフェクト番号
        const uint32_t effectTypeCount = GetPostEffectTypeCount(); // 選択可能なポストエフェクト種類数
        const char* effectNames[static_cast<uint32_t>(PostEffectType::Count)] {}; // 選択可能なエフェクト名
        for (uint32_t effectTypeIndex = 0; effectTypeIndex < effectTypeCount; ++effectTypeIndex) {
            effectNames[effectTypeIndex] = GetPostEffectTypeName(static_cast<PostEffectType>(effectTypeIndex));
        }

        if (ImGui::Combo(
                "Effect",
                &effectIndex,
                effectNames,
                static_cast<int>(effectTypeCount))) {
            ctx.postProcess->SetEffectType(
                static_cast<PostEffectType>(effectIndex));
        }
        if (ctx.postProcess->GetEffectType() == PostEffectType::BoxFilter) {
            int kernelIndex = ctx.postProcess->GetBoxFilterKernelSize() == kPostProcessKernelSize5x5
                ? kKernelIndex5x5
                : kKernelIndex3x3; // 現在のカーネル番号
            const char* kernelNames[] = {
                "3x3",
                "5x5"
            }; // 選択可能なカーネルサイズ

            if (ImGui::Combo(
                    "Kernel Size",
                    &kernelIndex,
                    kernelNames,
                    IM_ARRAYSIZE(kernelNames))) {
                uint32_t kernelSize = kernelIndex == kKernelIndex5x5
                    ? kPostProcessKernelSize5x5
                    : kPostProcessKernelSize3x3; // 選択されたカーネルサイズ
                ctx.postProcess->SetBoxFilterKernelSize(kernelSize);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::GaussianFilter) {
            uint32_t currentKernelSize = ctx.postProcess->GetGaussianKernelSize(); // 現在のカーネルサイズ
            int kernelIndex = currentKernelSize == kPostProcessKernelSize3x3
                ? kKernelIndex3x3
                : (currentKernelSize == kPostProcessKernelSize5x5 ? kKernelIndex5x5 : kKernelIndex7x7);
            const char* kernelNames[] = {
                "3x3",
                "5x5",
                "7x7"
            }; // 選択可能なカーネルサイズ

            if (ImGui::Combo(
                    "Gaussian Kernel",
                    &kernelIndex,
                    kernelNames,
                    IM_ARRAYSIZE(kernelNames))) {
                const uint32_t kernelSizes[] = {
                    kPostProcessKernelSize3x3,
                    kPostProcessKernelSize5x5,
                    kPostProcessKernelSize7x7
                }; // 選択値に対応するサイズ
                ctx.postProcess->SetGaussianKernelSize(kernelSizes[kernelIndex]);
            }

            float sigma = ctx.postProcess->GetGaussianSigma(); // 現在の標準偏差
            if (ImGui::DragFloat(
                    "Sigma",
                    &sigma,
                    kGaussianSigmaStep,
                    kPostProcessGaussianSigmaMin,
                    kPostProcessGaussianSigmaMax,
                    "%.2f")) {
                ctx.postProcess->SetGaussianSigma(sigma);
            }

            uint32_t sampleCount = ctx.postProcess->GetGaussianKernelSize() * kSeparablePassCount; // 分離処理の総サンプル数
            ImGui::Text("Samples: %u (separable 2-pass)", sampleCount);
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::LuminanceOutline
            || ctx.postProcess->GetEffectType() == PostEffectType::DepthOutline) {
            float outlineStrength = ctx.postProcess->GetOutlineStrength(); // 現在の輪郭強度
            if (ImGui::DragFloat(
                    "Outline Strength",
                    &outlineStrength,
                    kOutlineStrengthStep,
                    kPostProcessOutlineStrengthMin,
                    kPostProcessOutlineStrengthMax,
                    "%.1f")) {
                ctx.postProcess->SetOutlineStrength(outlineStrength);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::DepthOutline) {
            float depthThreshold = ctx.postProcess->GetDepthOutlineThreshold(); // 深度差の判定閾値
            if (ImGui::DragFloat(
                    "Depth Threshold",
                    &depthThreshold,
                    kDepthThresholdStep,
                    kPostProcessNormalizedValueMin,
                    kDepthThresholdMax,
                    "%.3f")) {
                ctx.postProcess->SetDepthOutlineThreshold(depthThreshold);
            }

            float depthSoftness = ctx.postProcess->GetDepthOutlineSoftness(); // 輪郭の立ち上がり幅
            if (ImGui::DragFloat(
                    "Depth Softness",
                    &depthSoftness,
                    kDepthSoftnessStep,
                    kDepthSoftnessMin,
                    kDepthSoftnessMax,
                    "%.3f")) {
                ctx.postProcess->SetDepthOutlineSoftness(depthSoftness);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::RadialBlur) {
            Math::Vector2 center = ctx.postProcess->GetRadialBlurCenter(); // 現在のブラー中心
            float centerValues[2] = {
                center.x,
                center.y
            }; // ImGui編集用中心座標
            if (ImGui::DragFloat2(
                    "Blur Center",
                    centerValues,
                    kEffectCenterStep,
                    kPostProcessNormalizedValueMin,
                    kPostProcessNormalizedValueMax,
                    "%.3f")) {
                ctx.postProcess->SetRadialBlurCenter(
                    { centerValues[0], centerValues[1] });
            }

            float blurWidth = ctx.postProcess->GetRadialBlurWidth(); // 現在のブラー幅
            if (ImGui::DragFloat(
                    "Blur Width",
                    &blurWidth,
                    kRadialBlurWidthStep,
                    kPostProcessNormalizedValueMin,
                    kPostProcessRadialBlurWidthMax,
                    "%.4f")) {
                ctx.postProcess->SetRadialBlurWidth(blurWidth);
            }

            int sampleCount = static_cast<int>(ctx.postProcess->GetRadialBlurSampleCount()); // サンプル数
            if (ImGui::SliderInt(
                    "Sample Count",
                    &sampleCount,
                    static_cast<int>(kPostProcessRadialBlurSampleMin),
                    static_cast<int>(kPostProcessRadialBlurSampleMax))) {
                ctx.postProcess->SetRadialBlurSampleCount(
                    static_cast<uint32_t>(sampleCount));
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Distortion) {
            Math::Vector2 center = ctx.postProcess->GetDistortionCenter(); // 現在の歪み中心
            float centerValues[2] = { center.x, center.y }; // ImGui編集用の歪み中心
            if (ImGui::DragFloat2(
                    "Distortion Center",
                    centerValues,
                    kEffectCenterStep,
                    kPostProcessNormalizedValueMin,
                    kPostProcessNormalizedValueMax,
                    "%.3f")) {
                ctx.postProcess->SetDistortionCenter(
                    { centerValues[0], centerValues[1] });
            }

            float strength = ctx.postProcess->GetDistortionStrength(); // 現在の歪み強度
            if (ImGui::DragFloat(
                    "Distortion Strength",
                    &strength,
                    kDistortionStrengthStep,
                    kPostProcessDistortionStrengthMin,
                    kPostProcessDistortionStrengthMax,
                    "%.3f")) {
                ctx.postProcess->SetDistortionStrength(strength);
            }

            float radius = ctx.postProcess->GetDistortionRadius(); // 現在の歪み半径
            if (ImGui::DragFloat(
                    "Distortion Radius",
                    &radius,
                    kDistortionRadiusStep,
                    kPostProcessDistortionRadiusMin,
                    kPostProcessDistortionRadiusMax,
                    "%.2f")) {
                ctx.postProcess->SetDistortionRadius(radius);
            }

            float waveCount = ctx.postProcess->GetDistortionWaveCount(); // 現在の歪み波数
            if (ImGui::DragFloat(
                    "Distortion Wave Count",
                    &waveCount,
                    kDistortionWaveStep,
                    kPostProcessNormalizedValueMin,
                    kPostProcessDistortionWaveCountMax,
                    "%.1f")) {
                ctx.postProcess->SetDistortionWaveCount(waveCount);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Dissolve) {
            float threshold = ctx.postProcess->GetDissolveThreshold(); // 現在のDissolve閾値
            if (ImGui::SliderFloat(
                    "Threshold",
                    &threshold,
                    kPostProcessNormalizedValueMin,
                    kPostProcessNormalizedValueMax,
                    "%.3f")) {
                ctx.postProcess->SetDissolveThreshold(threshold);
            }

            float edgeWidth = ctx.postProcess->GetDissolveEdgeWidth(); // 現在の境界幅
            if (ImGui::DragFloat(
                    "Edge Width",
                    &edgeWidth,
                    kDissolveEdgeWidthStep,
                    kPostProcessDissolveEdgeWidthMin,
                    kPostProcessDissolveEdgeWidthMax,
                    "%.3f")) {
                ctx.postProcess->SetDissolveEdgeWidth(edgeWidth);
            }

            Math::Vector3 edgeColor = ctx.postProcess->GetDissolveEdgeColor(); // 現在の境界色
            float edgeColorValues[3] = {
                edgeColor.x,
                edgeColor.y,
                edgeColor.z
            }; // ImGui編集用の境界色
            if (ImGui::ColorEdit3("Edge Color", edgeColorValues)) {
                ctx.postProcess->SetDissolveEdgeColor(
                    { edgeColorValues[0],
                        edgeColorValues[1],
                        edgeColorValues[2] });
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Random) {
            float randomStrength = ctx.postProcess->GetRandomStrength(); // 現在のノイズ強度
            if (ImGui::SliderFloat(
                    "Noise Strength",
                    &randomStrength,
                    kPostProcessNormalizedValueMin,
                    kPostProcessNormalizedValueMax,
                    "%.2f")) {
                ctx.postProcess->SetRandomStrength(randomStrength);
            }

            float randomScale = ctx.postProcess->GetRandomScale(); // 現在のノイズの細かさ
            if (ImGui::DragFloat(
                    "Noise Scale",
                    &randomScale,
                    kRandomScaleStep,
                    kPostProcessRandomScaleMin,
                    kPostProcessRandomScaleMax,
                    "%.0f")) {
                ctx.postProcess->SetRandomScale(randomScale);
            }

            float randomSpeed = ctx.postProcess->GetRandomSpeed(); // 現在の変化速度
            if (ImGui::DragFloat(
                    "Noise Speed",
                    &randomSpeed,
                    kRandomSpeedStep,
                    kPostProcessNormalizedValueMin,
                    kPostProcessRandomSpeedMax,
                    "%.2f")) {
                ctx.postProcess->SetRandomSpeed(randomSpeed);
            }
        }

        ImGui::SeparatorText("Status");
        ImGui::Text(
            "PostProcess: %s",
            ctx.postProcess->IsReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Copy PSO: %s",
            ctx.postProcess->IsCopyReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Grayscale PSO: %s",
            ctx.postProcess->IsGrayscaleReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Vignette PSO: %s",
            ctx.postProcess->IsVignetteReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Box Filter PSO: %s",
            ctx.postProcess->IsBoxFilterReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Gaussian Filter PSO: %s",
            ctx.postProcess->IsGaussianFilterReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Luminance Outline PSO: %s",
            ctx.postProcess->IsLuminanceOutlineReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Depth Outline PSO: %s",
            ctx.postProcess->IsDepthOutlineReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Radial Blur PSO: %s",
            ctx.postProcess->IsRadialBlurReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Dissolve PSO: %s",
            ctx.postProcess->IsDissolveReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Random PSO: %s",
            ctx.postProcess->IsRandomReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Distortion PSO: %s",
            ctx.postProcess->IsDistortionReady() ? "Ready" : "Not Ready");

        uint32_t srvIndex = ctx.postProcess->GetLastSrvIndex(); // 最後に描画した入力SRV
        if (srvIndex == UINT32_MAX) {
            ImGui::Text("Input SRV: Not Drawn");
        } else {
            ImGui::Text("Input SRV: %u", srvIndex);
        }

        ImGui::TextDisabled("Disabled uses the Copy pipeline.");
    }
#else
    (void)ctx;
#endif
}

} // namespace MyEngine
#endif