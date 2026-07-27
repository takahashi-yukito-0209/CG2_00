#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/PostProcess.h"
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace MyEngine {
namespace {
constexpr int kKernelIndex3x3 = 0; // 3x3を選択する番号
constexpr int kKernelIndex5x5 = 1; // 5x5を選択する番号
constexpr int kKernelIndex7x7 = 2; // 7x7を選択する番号
constexpr uint32_t kKernelSize3x3 = 3u; // 3x3カーネルサイズ
constexpr uint32_t kKernelSize5x5 = 5u; // 5x5カーネルサイズ
constexpr uint32_t kKernelSize7x7 = 7u; // 7x7カーネルサイズ
constexpr uint32_t kSeparablePassCount = 2u; // 分離フィルタのパス数
constexpr float kPostProcessNormalizedMin = 0.0f; // 正規化値の最小値
constexpr float kPostProcessNormalizedMax = 1.0f; // 正規化値の最大値
constexpr float kGaussianSigmaStep = 0.05f; // Gaussian sigmaの調整幅
constexpr float kGaussianSigmaMin = 0.1f; // Gaussian sigmaの最小値
constexpr float kGaussianSigmaMax = 10.0f; // Gaussian sigmaの最大値
constexpr float kOutlineStrengthStep = 0.1f; // アウトライン強度の調整幅
constexpr float kOutlineStrengthMin = 0.0f; // アウトライン強度の最小値
constexpr float kOutlineStrengthMax = 32.0f; // アウトライン強度の最大値
constexpr float kDepthThresholdStep = 0.001f; // 深度閾値の調整幅
constexpr float kDepthThresholdMax = 0.2f; // 深度閾値の最大値
constexpr float kDepthSoftnessStep = 0.001f; // 深度アウトライン柔らかさの調整幅
constexpr float kDepthSoftnessMin = 0.001f; // 深度アウトライン柔らかさの最小値
constexpr float kDepthSoftnessMax = 0.2f; // 深度アウトライン柔らかさの最大値
constexpr float kEffectCenterStep = 0.005f; // 中心座標の調整幅
constexpr float kRadialBlurWidthStep = 0.0005f; // ラジアルブラー幅の調整幅
constexpr float kRadialBlurWidthMax = 0.1f; // ラジアルブラー幅の最大値
constexpr int kRadialBlurSampleMin = 1; // ラジアルブラーサンプル数の最小値
constexpr int kRadialBlurSampleMax = 32; // ラジアルブラーサンプル数の最大値
constexpr float kDistortionStrengthStep = 0.001f; // 歪み強度の調整幅
constexpr float kDistortionStrengthMin = -0.1f; // 歪み強度の最小値
constexpr float kDistortionStrengthMax = 0.1f; // 歪み強度の最大値
constexpr float kDistortionRadiusStep = 0.01f; // 歪み半径の調整幅
constexpr float kDistortionRadiusMin = 0.01f; // 歪み半径の最小値
constexpr float kDistortionRadiusMax = 1.5f; // 歪み半径の最大値
constexpr float kDistortionWaveStep = 0.1f; // 歪み波数の調整幅
constexpr float kDistortionWaveMax = 12.0f; // 歪み波数の最大値
constexpr float kDissolveEdgeWidthStep = 0.001f; // Dissolve境界幅の調整幅
constexpr float kDissolveEdgeWidthMin = 0.001f; // Dissolve境界幅の最小値
constexpr float kDissolveEdgeWidthMax = 0.25f; // Dissolve境界幅の最大値
constexpr float kRandomScaleStep = 1.0f; // ノイズスケールの調整幅
constexpr float kRandomScaleMin = 1.0f; // ノイズスケールの最小値
constexpr float kRandomScaleMax = 2000.0f; // ノイズスケールの最大値
constexpr float kRandomSpeedStep = 0.05f; // ノイズ速度の調整幅
constexpr float kRandomSpeedMax = 20.0f; // ノイズ速度の最大値
/// <summary>
/// パス文字列から表示用のファイル名だけを取得する。
/// </summary>
std::string GetDisplayFileName(const std::string& path)
{
    const size_t slashPosition = path.find_last_of("/\\"); // 最後の区切り文字位置
    if (slashPosition == std::string::npos) {
        return path;
    }
    return path.substr(slashPosition + 1);
}

/// <summary>
/// 3DオブジェクトのImGui表示名を作成する。
/// </summary>
std::string BuildObjectDisplayLabel(const Object3d* object, int objectIndex)
{
    std::string label = "Object " + std::to_string(objectIndex); // indexベースの表示名
    if (object) {
        label += " [ID " + std::to_string(object->GetObjectId()) + "]";
        const std::string displayName = GetDisplayFileName(object->GetDebugName()); // モデル由来の表示名
        if (!displayName.empty()) {
            label += " : " + displayName;
        }
    }
    return label;
}

/// <summary>
/// スプライトのImGui表示名を作成する。
/// </summary>
std::string BuildSpriteDisplayLabel(const Sprite* sprite, int spriteIndex)
{
    std::string label = "Sprite " + std::to_string(spriteIndex); // indexベースの表示名
    if (sprite) {
        label += " [ID " + std::to_string(sprite->GetSpriteId()) + "]";
        const std::string displayName = GetDisplayFileName(sprite->GetTextureFilePath()); // テクスチャ由来の表示名
        if (!displayName.empty()) {
            label += " : " + displayName;
        }
    }
    return label;
}

/// <summary>
/// パーティクルエミッターのImGui表示名を作成する。
/// </summary>
std::string BuildParticleEmitterDisplayLabel(const ParticleEmitter* emitter, int emitterIndex)
{
    std::string label = "Emitter " + std::to_string(emitterIndex); // indexベースの表示名
    if (emitter) {
        label += " [ID " + std::to_string(emitter->GetEmitterId()) + "]";
        if (!emitter->groupName.empty()) {
            label += " : " + emitter->groupName;
        }
    }
    return label;
}
} // namespace

/// <summary>
/// シーン情報のImGuiを描画する
/// </summary>
void ImGuiManager::DrawSceneSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ctx.currentSceneName) {
            ImGui::Text("Current Scene: %s", ctx.currentSceneName);
        }

        if (ctx.requestSceneChange) {
            const char* sceneNames[] = {
                "Title",
                "Play",
            }; // ImGuiから切り替え可能なシーン名
            constexpr int sceneCount = static_cast<int>(sizeof(sceneNames) / sizeof(sceneNames[0])); // シーン数
            int selectedSceneIndex = 0; // 現在選択されているシーン番号
            for (int sceneIndex = 0; sceneIndex < sceneCount; ++sceneIndex) {
                if (ctx.currentSceneName && std::strcmp(ctx.currentSceneName, sceneNames[sceneIndex]) == 0) {
                    selectedSceneIndex = sceneIndex;
                    break;
                }
            }
            if (ImGui::Combo("Scene##SceneSelector", &selectedSceneIndex, sceneNames, sceneCount)) {
                ctx.requestSceneChange(sceneNames[selectedSceneIndex]);
            }
        }
        ImGui::Separator();
        const char* targetLabels[] = {
            "Object3D",
            "Sprite2D",
            "Emitter",
            "GPU Emitter"
        }; // 選択可能なギズモ対象
        gizmoTargetMode_ = (std::clamp)(gizmoTargetMode_, 0, static_cast<int>(IM_ARRAYSIZE(targetLabels)) - 1);
        ImGui::Combo("Gizmo Target", &gizmoTargetMode_, targetLabels, IM_ARRAYSIZE(targetLabels));

        if (gizmoTargetMode_ == 3) {
            const char* gpuOperationLabels[] = {
                "Move",
                "Scale"
            }; // GPU Emitterで選択可能なギズモ操作
            int gpuOperationIndex = gizmoOperationMode_ == 2 ? 1 : 0; // GPU Emitter用の操作番号
            if (ImGui::Combo("Gizmo Operation", &gpuOperationIndex, gpuOperationLabels, IM_ARRAYSIZE(gpuOperationLabels))) {
                gizmoOperationMode_ = gpuOperationIndex == 0 ? 0 : 2;
            }
            if (gizmoOperationMode_ != 2) {
                gizmoOperationMode_ = 0;
            }
        } else {
            const char* operationLabels[] = {
                "Move",
                "Rotate",
                "Scale"
            }; // 選択可能なギズモ操作
            gizmoOperationMode_ = (std::clamp)(gizmoOperationMode_, 0, static_cast<int>(IM_ARRAYSIZE(operationLabels)) - 1);
            ImGui::Combo("Gizmo Operation", &gizmoOperationMode_, operationLabels, IM_ARRAYSIZE(operationLabels));
        }

        if (gizmoTargetMode_ == 0 || gizmoTargetMode_ == 2) {
            const char* spaceLabels[] = {
                "Global",
                "Local"
            }; // 選択可能なギズモ座標空間
            gizmoTransformSpaceMode_ = (std::clamp)(gizmoTransformSpaceMode_, 0, static_cast<int>(IM_ARRAYSIZE(spaceLabels)) - 1);
            ImGui::Combo("Gizmo Space", &gizmoTransformSpaceMode_, spaceLabels, IM_ARRAYSIZE(spaceLabels));
        }

        DirectXCommon* directXCommon = DirectXCommon::GetInstance(); // フレーム同期設定の反映先
        if (directXCommon) {
            const char* frameSyncLabels[] = {
                "VSync",
                "Fixed 60",
                "Unlimited"
            }; // 選択可能なフレーム同期モード
            int frameSyncIndex = static_cast<int>(directXCommon->GetFrameSyncMode()); // ImGui編集用の同期モード番号
            frameSyncIndex = (std::clamp)(frameSyncIndex, 0, static_cast<int>(IM_ARRAYSIZE(frameSyncLabels)) - 1);
            if (ImGui::Combo("Frame Sync", &frameSyncIndex, frameSyncLabels, IM_ARRAYSIZE(frameSyncLabels))) {
                directXCommon->SetFrameSyncMode(static_cast<DirectXCommon::FrameSyncMode>(frameSyncIndex));
            }
        }

        const float frameRate = ImGui::GetIO().Framerate; // 現在のImGui計測FPS
        const float frameTimeMs = frameRate > 0.0f ? 1000.0f / frameRate : 0.0f; // 1フレームあたりの表示時間
        ImGui::Text("FPS: %.1f (%.3f ms)", frameRate, frameTimeMs);
    }
#else
    (void)ctx;
#endif
}

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
            int kernelIndex = ctx.postProcess->GetBoxFilterKernelSize() == kKernelSize5x5
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
                    ? kKernelSize5x5
                    : kKernelSize3x3; // 選択されたカーネルサイズ
                ctx.postProcess->SetBoxFilterKernelSize(kernelSize);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::GaussianFilter) {
            uint32_t currentKernelSize = ctx.postProcess->GetGaussianKernelSize(); // 現在のカーネルサイズ
            int kernelIndex = currentKernelSize == kKernelSize3x3
                ? kKernelIndex3x3
                : (currentKernelSize == kKernelSize5x5 ? kKernelIndex5x5 : kKernelIndex7x7);
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
                    kKernelSize3x3,
                    kKernelSize5x5,
                    kKernelSize7x7
                }; // 選択値に対応するサイズ
                ctx.postProcess->SetGaussianKernelSize(kernelSizes[kernelIndex]);
            }

            float sigma = ctx.postProcess->GetGaussianSigma(); // 現在の標準偏差
            if (ImGui::DragFloat(
                    "Sigma",
                    &sigma,
                    kGaussianSigmaStep,
                    kGaussianSigmaMin,
                    kGaussianSigmaMax,
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
                    kOutlineStrengthMin,
                    kOutlineStrengthMax,
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
                    kPostProcessNormalizedMin,
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
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.3f")) {
                ctx.postProcess->SetRadialBlurCenter(
                    { centerValues[0], centerValues[1] });
            }

            float blurWidth = ctx.postProcess->GetRadialBlurWidth(); // 現在のブラー幅
            if (ImGui::DragFloat(
                    "Blur Width",
                    &blurWidth,
                    kRadialBlurWidthStep,
                    kPostProcessNormalizedMin,
                    kRadialBlurWidthMax,
                    "%.4f")) {
                ctx.postProcess->SetRadialBlurWidth(blurWidth);
            }

            int sampleCount = static_cast<int>(ctx.postProcess->GetRadialBlurSampleCount()); // サンプル数
            if (ImGui::SliderInt(
                    "Sample Count",
                    &sampleCount,
                    kRadialBlurSampleMin,
                    kRadialBlurSampleMax)) {
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
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.3f")) {
                ctx.postProcess->SetDistortionCenter(
                    { centerValues[0], centerValues[1] });
            }

            float strength = ctx.postProcess->GetDistortionStrength(); // 現在の歪み強度
            if (ImGui::DragFloat(
                    "Distortion Strength",
                    &strength,
                    kDistortionStrengthStep,
                    kDistortionStrengthMin,
                    kDistortionStrengthMax,
                    "%.3f")) {
                ctx.postProcess->SetDistortionStrength(strength);
            }

            float radius = ctx.postProcess->GetDistortionRadius(); // 現在の歪み半径
            if (ImGui::DragFloat(
                    "Distortion Radius",
                    &radius,
                    kDistortionRadiusStep,
                    kDistortionRadiusMin,
                    kDistortionRadiusMax,
                    "%.2f")) {
                ctx.postProcess->SetDistortionRadius(radius);
            }

            float waveCount = ctx.postProcess->GetDistortionWaveCount(); // 現在の歪み波数
            if (ImGui::DragFloat(
                    "Distortion Wave Count",
                    &waveCount,
                    kDistortionWaveStep,
                    kPostProcessNormalizedMin,
                    kDistortionWaveMax,
                    "%.1f")) {
                ctx.postProcess->SetDistortionWaveCount(waveCount);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Dissolve) {
            float threshold = ctx.postProcess->GetDissolveThreshold(); // 現在のDissolve閾値
            if (ImGui::SliderFloat(
                    "Threshold",
                    &threshold,
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.3f")) {
                ctx.postProcess->SetDissolveThreshold(threshold);
            }

            float edgeWidth = ctx.postProcess->GetDissolveEdgeWidth(); // 現在の境界幅
            if (ImGui::DragFloat(
                    "Edge Width",
                    &edgeWidth,
                    kDissolveEdgeWidthStep,
                    kDissolveEdgeWidthMin,
                    kDissolveEdgeWidthMax,
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
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.2f")) {
                ctx.postProcess->SetRandomStrength(randomStrength);
            }

            float randomScale = ctx.postProcess->GetRandomScale(); // 現在のノイズの細かさ
            if (ImGui::DragFloat(
                    "Noise Scale",
                    &randomScale,
                    kRandomScaleStep,
                    kRandomScaleMin,
                    kRandomScaleMax,
                    "%.0f")) {
                ctx.postProcess->SetRandomScale(randomScale);
            }

            float randomSpeed = ctx.postProcess->GetRandomSpeed(); // 現在の変化速度
            if (ImGui::DragFloat(
                    "Noise Speed",
                    &randomSpeed,
                    kRandomSpeedStep,
                    kPostProcessNormalizedMin,
                    kRandomSpeedMax,
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

/// <summary>
/// パーティクル関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawParticleSection(Context& ctx)
{
#ifdef USE_IMGUI
    const bool hasCpuEmitters = ctx.particleEmitters && !ctx.particleEmitters->empty(); // CPUエミッター一覧があるか
    if (!hasCpuEmitters && !ctx.particleEmitter && !ctx.particleManager) {
        return;
    }

    if (ImGui::CollapsingHeader("Particle")) {
        if (hasCpuEmitters) {
            const int emitterCount = static_cast<int>(ctx.particleEmitters->size()); // 編集可能なCPUエミッター数
            selectedEmitterIndex_ = (std::clamp)(selectedEmitterIndex_, 0, emitterCount - 1);
            ParticleEmitter* previewEmitter = (*ctx.particleEmitters)[selectedEmitterIndex_]; // コンボの現在選択エミッター
            std::string preview = BuildParticleEmitterDisplayLabel(previewEmitter, selectedEmitterIndex_); // コンボの現在表示名

            if (ImGui::BeginCombo("CPU Emitter Target", preview.c_str())) {
                for (int emitterIndex = 0; emitterIndex < emitterCount; ++emitterIndex) {
                    ParticleEmitter* emitter = (*ctx.particleEmitters)[emitterIndex]; // 表示名を作るエミッター
                    std::string label = BuildParticleEmitterDisplayLabel(emitter, emitterIndex); // 選択候補の表示名
                    const bool isSelected = selectedEmitterIndex_ == emitterIndex; // 現在選択中か
                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        selectedEmitterIndex_ = emitterIndex;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ParticleEmitter* selectedEmitter = (*ctx.particleEmitters)[selectedEmitterIndex_]; // 編集対象のCPUエミッター
            if (selectedEmitter) {
                ImGui::PushID(selectedEmitter);
                selectedEmitter->DrawImGui();
                ImGui::PopID();
            }
        } else if (ctx.particleEmitter) {
            ImGui::PushID(ctx.particleEmitter);
            ctx.particleEmitter->DrawImGui();
            ImGui::PopID();
        }

        ImGui::Separator();

        if (ctx.particleManager) {
            ctx.particleManager->DrawImGui(ctx.postProcess);
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// 3Dオブジェクト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawObjectSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.objects3d || ctx.objects3d->empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Objects")) {
        const int objectCount = static_cast<int>(ctx.objects3d->size()); // 表示可能な3Dオブジェクト数
        selectedObjectIndex_ = (std::clamp)(selectedObjectIndex_, 0, objectCount - 1);

        Object3d* previewObject = (*ctx.objects3d)[selectedObjectIndex_]; // コンボで現在選択している3Dオブジェクト
        std::string preview = BuildObjectDisplayLabel(previewObject, selectedObjectIndex_); // コンボの現在表示名

        if (ImGui::BeginCombo("Target", preview.c_str())) {
            for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
                Object3d* object = (*ctx.objects3d)[objectIndex]; // 表示名を取得する3Dオブジェクト
                std::string label = BuildObjectDisplayLabel(object, objectIndex); // 選択候補の表示名

                const bool isSelected = selectedObjectIndex_ == objectIndex; // 現在選択中か
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedObjectIndex_ = objectIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        Object3d* selectedObject = (*ctx.objects3d)[selectedObjectIndex_]; // 編集対象の3Dオブジェクト
        if (selectedObject) {
            ImGui::Separator();
            selectedObject->DrawImGui(selectedObjectIndex_);
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// スプライト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawSpriteSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.sprites || ctx.sprites->empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Sprites")) {
        const int spriteCount = static_cast<int>(ctx.sprites->size()); // 表示可能なスプライト数
        selectedSpriteIndex_ = (std::clamp)(selectedSpriteIndex_, 0, spriteCount - 1);

        Sprite* previewSprite = (*ctx.sprites)[selectedSpriteIndex_]; // コンボで現在選択しているスプライト
        std::string preview = BuildSpriteDisplayLabel(previewSprite, selectedSpriteIndex_); // コンボの現在表示名

        if (ImGui::BeginCombo("Target", preview.c_str())) {
            for (int spriteIndex = 0; spriteIndex < spriteCount; ++spriteIndex) {
                Sprite* sprite = (*ctx.sprites)[spriteIndex]; // 表示名を取得するスプライト
                std::string label = BuildSpriteDisplayLabel(sprite, spriteIndex); // 選択候補の表示名

                const bool isSelected = selectedSpriteIndex_ == spriteIndex; // 現在選択中か
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedSpriteIndex_ = spriteIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        Sprite* selectedSprite = (*ctx.sprites)[selectedSpriteIndex_]; // 編集対象のスプライト
        if (selectedSprite) {
            ImGui::Separator();
            selectedSprite->DrawImGui();
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// 共通設定関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawCommonSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Common")) {
        if (ctx.spriteCommon || ctx.object3dCommon) {
            if (ImGui::CollapsingHeader("Blend Mode")) {
                const char* blendNames[] = {
                    "None",
                    "Alpha",
                    "Add",
                    "Subtract",
                    "Multiply",
                    "Screen"
                }; // 選択可能なブレンドモード名

                if (ctx.object3dCommon) {
                    int objectBlendIndex = static_cast<int>(ctx.object3dCommon->GetBlendMode()); // 3Dオブジェクトのブレンドモード
                    if (ImGui::Combo("1. Object3D Blend", &objectBlendIndex, blendNames, IM_ARRAYSIZE(blendNames))) {
                        ctx.object3dCommon->SetBlendMode(static_cast<BlendMode>(objectBlendIndex));
                    }
                }

                if (ctx.spriteCommon) {
                    int spriteBlendIndex = static_cast<int>(ctx.spriteCommon->GetBlendMode()); // スプライトのブレンドモード
                    if (ImGui::Combo("2. Sprite Blend", &spriteBlendIndex, blendNames, IM_ARRAYSIZE(blendNames))) {
                        ctx.spriteCommon->SetBlendMode(static_cast<BlendMode>(spriteBlendIndex));
                    }
                }
            }
        }

        if (ctx.object3dCommon) {
            ctx.object3dCommon->DrawImGui();
        }

        if (ctx.spriteCommon) {
            ctx.spriteCommon->DrawImGui();
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// カメラ関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawCameraWindow(Context& ctx)
{
#ifdef USE_IMGUI
    ImGui::Begin("Camera");

    if (ctx.object3dCommon) {
        ctx.object3dCommon->DrawCameraImGui();
    }

    ImGui::End();
#else
    (void)ctx;
#endif
}

} // namespace MyEngine
#endif