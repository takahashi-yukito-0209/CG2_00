#include "ParticleEmitter.h"
#include "ParticleEditorImGuiUtility.h"
#include "engine/particle/ParticleManager.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

using namespace MyEngine;
using namespace MyEngine::ParticleEditorImGuiUtility;

namespace {
constexpr float kImGuiTranslateStep = 0.01f; // 発生位置の調整幅
constexpr float kImGuiRotateStep = 0.01f; // 発生回転の調整幅
constexpr float kImGuiScaleStep = 0.01f; // 発生スケールの調整幅
constexpr float kImGuiRangeStep = 0.01f; // デバッグ範囲の調整幅
constexpr int kImGuiCountStep = 1; // 発生数の調整幅
constexpr int kImGuiCountMin = 0; // 発生数の最小値
constexpr int kImGuiCountMax = 1000; // 発生数の最大値
constexpr float kImGuiFrequencyStep = 0.01f; // 発生間隔の調整幅
constexpr float kImGuiFrequencyMin = 0.0f; // 発生間隔の最小値
constexpr float kImGuiFrequencyMax = 100.0f; // 発生間隔の最大値
constexpr float kAlwaysEmitFrequencyThreshold = 0.0f; // 毎フレーム発生に切り替える発生間隔の下限
constexpr float kEmitterElapsedTimeStart = 0.0f; // 発生経過時間の初期値
} // namespace

#ifdef USE_IMGUI
static_assert(true, "ImGui available");
#endif

/// <summary>
/// 現在の設定に従ってパーティクルを発生させる。
/// </summary>
void ParticleEmitter::Emit()
{
    // グループ未設定なら発生させない
    if (groupName.empty()) {
        return;
    }

    if (useCylinderEffect) {
        ParticleManager::GetInstance()->EmitCylinderEffect(groupName, transform.translate, count);
    } else if (useRingEffect) {
        ParticleManager::GetInstance()->EmitRingEffect(groupName, transform.translate, count);
    } else if (useHitEffect) {
        ParticleManager::GetInstance()->EmitHitEffect(groupName, transform.translate, count);
    } else {
        ParticleManager::GetInstance()->Emit(groupName, transform.translate, count);
    }
}

/// <summary>
/// 経過時間を進め、発生間隔を超えた分だけ発生させる。
/// </summary>
void ParticleEmitter::Update(float deltaTime)
{
    // 経過時間を進める
    elapsed += deltaTime;
    // 発生間隔が0以下なら毎フレーム発生させる
    if (frequency <= kAlwaysEmitFrequencyThreshold) {
        if (count) {
            Emit();
        }
        elapsed = kEmitterElapsedTimeStart;
        return;
    }
    // 発生間隔を超えたら発生させる
    while (elapsed >= frequency) {
        Emit();
        elapsed -= frequency; // 余剰も考慮
    }
}

/// <summary>
/// ImGuiでエミッター設定を編集する。
/// </summary>
void ParticleEmitter::DrawImGui()
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Emission", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawParticleTextInput("Group Name", "##GroupName", groupName);
        int tmpCount = static_cast<int>(count); // ImGuiで編集する発生数
        DrawParticlePropertyLabel("Particles / Emit");
        if (ImGui::DragInt("##Count", &tmpCount, kImGuiCountStep, kImGuiCountMin, kImGuiCountMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            count = static_cast<uint32_t>(tmpCount);
        }
        DrawParticlePropertyLabel("Emit Interval (s)");
        ImGui::DragFloat("##Frequency", &frequency, kImGuiFrequencyStep, kImGuiFrequencyMin, kImGuiFrequencyMax);
    }
    if (ImGui::CollapsingHeader("Transform")) {
        DrawParticlePropertyLabel("Scale");
        ImGui::DragFloat3("##Scale", &transform.scale.x, kImGuiScaleStep);
        DrawParticlePropertyLabel("Rotation");
        ImGui::DragFloat3("##Rotate", &transform.rotate.x, kImGuiRotateStep);
        DrawParticlePropertyLabel("Position");
        ImGui::DragFloat3("##Translate", &transform.translate.x, kImGuiTranslateStep);
    }
    if (ImGui::CollapsingHeader("Effect Type")) {
        ImGui::Checkbox("Use Hit Effect", &useHitEffect);
        ImGui::Checkbox("Use Ring Effect", &useRingEffect);
        ImGui::Checkbox("Use Cylinder Effect", &useCylinderEffect);
    }
    if (ImGui::CollapsingHeader("Debug Range")) {
        ImGui::Checkbox("Show Debug Range", &showDebugRange);
        ImGui::BeginDisabled(!showDebugRange);
        DrawParticlePropertyLabel("Range Half Size");
        ImGui::DragFloat3("##DebugRangeHalf", &debugRangeHalfSize.x, kImGuiRangeStep, 0.0f, 100.0f);
        DrawParticlePropertyLabel("Grid Half Lines");
        ImGui::DragInt("##DebugGridHalfLines", &debugGridHalfLineCount, 1, 1, 64);
        DrawParticlePropertyLabel("Grid Spacing");
        ImGui::DragFloat("##DebugGridSpacing", &debugGridSpacing, kImGuiRangeStep, 0.01f, 100.0f);
        ImGui::EndDisabled();
    }
#endif
}
