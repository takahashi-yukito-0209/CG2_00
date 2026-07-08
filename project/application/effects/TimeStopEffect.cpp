#include "TimeStopEffect.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../../engine/particle/ParticleManager.h"

#include <algorithm>
#include <cstdint>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr float kMinimumEffectDuration = 0.0001f; // 演出時間のゼロ除算を防ぐ最小値
constexpr uint32_t kStartCylinderCount = 1; // 開始時に発生させる円柱エフェクト数
}

/// <summary>
/// エフェクトの進行状態を初期状態へ戻す
/// </summary>
void TimeStopEffect::ResetState()
{
    phase_ = TimeStopPhase::Idle;
    phaseTime_ = 0.0f;
    previousPostEffect_ = PostEffectType::Copy;
}

/// <summary>
/// 調整値を初期値へ戻す
/// </summary>
void TimeStopEffect::ResetSettings()
{
    settings_ = TimeStopSettings {};
}

/// <summary>
/// 時間停止開始時の歪みポストエフェクトを設定する。
/// </summary>
void TimeStopEffect::ConfigureDistortionPostProcess(PostProcess& postProcess, const Vector2& effectCenter)
{
    postProcess.SetDistortionCenter(effectCenter);
    postProcess.SetRadialBlurCenter(effectCenter);
    postProcess.SetDistortionRadius(settings_.distortionRadius);
    postProcess.SetDistortionWaveCount(settings_.distortionWaveCount);
    postProcess.SetDistortionStrength(0.0f);
    postProcess.SetDistortionProgress(0.0f);
    postProcess.SetEffectType(PostEffectType::Distortion);
}

/// <summary>
/// 開始フェーズのポストエフェクト状態を反映する。
/// </summary>
void TimeStopEffect::ApplyEnteringPostProcess(PostProcess& postProcess, float progress)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetDistortionStrength(-settings_.distortionStrength * progress);
    postProcess.SetDistortionProgress(progress);
}

/// <summary>
/// 停止フェーズのポストエフェクト状態を反映する。
/// </summary>
void TimeStopEffect::ApplyStoppedPostProcess(PostProcess& postProcess, bool resetDistortionStrength)
{
    postProcess.SetEffectType(PostEffectType::Grayscale);
    if (resetDistortionStrength) {
        postProcess.SetDistortionStrength(0.0f);
    }
}

/// <summary>
/// 再開フェーズのポストエフェクト状態を反映する。
/// </summary>
void TimeStopEffect::ApplyReleasingPostProcess(PostProcess& postProcess, float progress)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetDistortionStrength(settings_.distortionStrength * (1.0f - progress));
    postProcess.SetDistortionProgress(progress);
}

/// <summary>
/// 時間停止エフェクトを開始する
/// </summary>
void TimeStopEffect::Start(PostProcess& postProcess, const Vector2& effectCenter)
{
    previousPostEffect_ = postProcess.GetEffectType(); // 再生前のポストエフェクト
    phase_ = TimeStopPhase::Entering;
    phaseTime_ = 0.0f;

    ConfigureDistortionPostProcess(postProcess, effectCenter);

    ParticleManager* particleManager = ParticleManager::GetInstance(); // 開始演出を発生させるパーティクル管理
    if (particleManager) {
        particleManager->EmitCylinderEffect("Cylinder", settings_.effectPosition, kStartCylinderCount);
    }
}

/// <summary>
/// 時間停止エフェクトの状態を更新する
/// </summary>
void TimeStopEffect::Update(float deltaTime, PostProcess& postProcess)
{
    if (phase_ == TimeStopPhase::Idle) {
        return;
    }

    phaseTime_ += deltaTime;
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 時間停止演出を発生させるパーティクル管理

    switch (phase_) {
    case TimeStopPhase::Idle:
        break;

    case TimeStopPhase::Entering: {
        const float duration = (std::max)(settings_.enterDuration, kMinimumEffectDuration); // 開始演出時間
        const float progress = (std::clamp)(phaseTime_ / duration, 0.0f, 1.0f); // 開始演出の進行度
        ApplyEnteringPostProcess(postProcess, progress);

        if (progress >= 1.0f) {
            phase_ = TimeStopPhase::Stopped;
            phaseTime_ = 0.0f;
            ApplyStoppedPostProcess(postProcess, true);
            if (particleManager) {
                particleManager->EmitRingEffect(
                    "Ring",
                    settings_.effectPosition,
                    static_cast<uint32_t>((std::max)(settings_.startRingCount, 0)));
            }
        }
        break;
    }

    case TimeStopPhase::Stopped:
        ApplyStoppedPostProcess(postProcess, false);
        if (phaseTime_ >= settings_.stopDuration) {
            phase_ = TimeStopPhase::Releasing;
            phaseTime_ = 0.0f;
            if (particleManager) {
                particleManager->EmitRingEffect(
                    "Ring",
                    settings_.effectPosition,
                    static_cast<uint32_t>((std::max)(settings_.releaseRingCount, 0)));
                particleManager->EmitHitEffect(
                    "Hit",
                    settings_.effectPosition,
                    static_cast<uint32_t>((std::max)(settings_.releaseFragmentCount, 0)));
            }
        }
        break;

    case TimeStopPhase::Releasing: {
        const float duration = (std::max)(settings_.releaseDuration, kMinimumEffectDuration); // 再開演出時間
        const float progress = (std::clamp)(phaseTime_ / duration, 0.0f, 1.0f); // 再開演出の進行度
        ApplyReleasingPostProcess(postProcess, progress);

        if (progress >= 1.0f) {
            phase_ = TimeStopPhase::Idle;
            phaseTime_ = 0.0f;
            postProcess.SetDistortionStrength(0.0f);
            postProcess.SetEffectType(previousPostEffect_);
        }
        break;
    }
    }
}

/// <summary>
/// ImGuiで時間停止エフェクトの状態と調整値を表示する
/// </summary>
void TimeStopEffect::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Time Stop Phase: %s", GetPhaseName());
    ImGui::Text("Phase Time: %.3f", phaseTime_);
    if (ImGui::Button("Reset Time Stop Settings")) {
        ResetSettings();
    }

    if (ImGui::CollapsingHeader("Time Stop Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Enter Duration", &settings_.enterDuration, 0.01f, 0.01f, 3.0f, "%.2f sec");
        ImGui::DragFloat("Stop Duration", &settings_.stopDuration, 0.05f, 0.05f, 10.0f, "%.2f sec");
        ImGui::DragFloat("Release Duration", &settings_.releaseDuration, 0.01f, 0.01f, 3.0f, "%.2f sec");
        ImGui::SeparatorText("Distortion");
        ImGui::DragFloat("Stop Distortion Strength", &settings_.distortionStrength, 0.001f, 0.0f, 0.2f, "%.3f");
        ImGui::DragFloat("Stop Distortion Radius", &settings_.distortionRadius, 0.01f, 0.05f, 1.5f, "%.2f");
        ImGui::DragFloat("Stop Distortion Waves", &settings_.distortionWaveCount, 0.1f, 1.0f, 12.0f, "%.1f");
        ImGui::SeparatorText("Particles");
        ImGui::SliderInt("Start Ring Count", &settings_.startRingCount, 0, 8);
        ImGui::SliderInt("Release Ring Count", &settings_.releaseRingCount, 0, 8);
        ImGui::SliderInt("Release Fragment Count", &settings_.releaseFragmentCount, 0, 64);
        ImGui::DragFloat3("Time Stop Position", &settings_.effectPosition.x, 0.05f);
    }
#endif
}

/// <summary>
/// 時間停止エフェクトが再生中か確認する
/// </summary>
bool TimeStopEffect::IsPlaying() const
{
    return phase_ != TimeStopPhase::Idle;
}

/// <summary>
/// 時間停止中か確認する
/// </summary>
bool TimeStopEffect::IsStopped() const
{
    return phase_ == TimeStopPhase::Stopped;
}

/// <summary>
/// エフェクト発生位置を取得する
/// </summary>
const Vector3& TimeStopEffect::GetEffectPosition() const
{
    return settings_.effectPosition;
}

/// <summary>
/// 現在のフェーズ名を取得する
/// </summary>
const char* TimeStopEffect::GetPhaseName() const
{
    switch (phase_) {
    case TimeStopPhase::Idle:
        return "Idle";
    case TimeStopPhase::Entering:
        return "Entering";
    case TimeStopPhase::Stopped:
        return "Stopped";
    case TimeStopPhase::Releasing:
        return "Releasing";
    }

    return "Unknown";
}
