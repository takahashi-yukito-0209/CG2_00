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
constexpr float kEffectTimeStart = 0.0f; // フェーズ開始時の経過時間
constexpr float kEffectProgressMin = 0.0f; // 演出進行率の最小値
constexpr float kEffectProgressMax = 1.0f; // 演出進行率の最大値
constexpr uint32_t kStartCylinderCount = 1; // 開始時に発生させる円柱エフェクト数
constexpr const char* kCylinderParticleGroupName = "Cylinder"; // 開始時に発生させる円柱エフェクト名
constexpr const char* kRingParticleGroupName = "Ring"; // リングエフェクト名
constexpr const char* kHitParticleGroupName = "Hit"; // 破片エフェクト名
constexpr float kImGuiShortDurationStep = 0.01f; // 短い時間設定の調整幅
constexpr float kImGuiStopDurationStep = 0.05f; // 停止時間設定の調整幅
constexpr float kImGuiMinimumShortDuration = 0.01f; // 短い時間設定の最小値
constexpr float kImGuiMinimumStopDuration = 0.05f; // 停止時間設定の最小値
constexpr float kImGuiMaximumShortDuration = 3.0f; // 短い時間設定の最大値
constexpr float kImGuiMaximumStopDuration = 10.0f; // 停止時間設定の最大値
constexpr float kImGuiDistortionStrengthStep = 0.001f; // 歪み強度の調整幅
constexpr float kImGuiDistortionStrengthMin = 0.0f; // 歪み強度の最小値
constexpr float kImGuiDistortionStrengthMax = 0.2f; // 歪み強度の最大値
constexpr float kImGuiDistortionRadiusStep = 0.01f; // 歪み範囲の調整幅
constexpr float kImGuiDistortionRadiusMin = 0.05f; // 歪み範囲の最小値
constexpr float kImGuiDistortionRadiusMax = 1.5f; // 歪み範囲の最大値
constexpr float kImGuiDistortionWaveStep = 0.1f; // 歪み波数の調整幅
constexpr float kImGuiDistortionWaveMin = 1.0f; // 歪み波数の最小値
constexpr float kImGuiDistortionWaveMax = 12.0f; // 歪み波数の最大値
constexpr int kImGuiRingCountMin = 0; // リング数の最小値
constexpr int kImGuiRingCountMax = 8; // リング数の最大値
constexpr int kImGuiFragmentCountMin = 0; // 破片数の最小値
constexpr int kImGuiFragmentCountMax = 64; // 破片数の最大値
constexpr float kImGuiPositionStep = 0.05f; // 発生位置の調整幅

/// <summary>
/// 演出時間をゼロ除算しない値へ補正する
/// </summary>
float GetSafeEffectDuration(float duration)
{
    const float safeDuration = (std::max)(duration, kMinimumEffectDuration); // 進行率計算に使用する演出時間
    return safeDuration;
}

/// <summary>
/// 経過時間から0から1の演出進行率を計算する
/// </summary>
float CalculateEffectProgress(float elapsedTime, float duration)
{
    const float safeDuration = GetSafeEffectDuration(duration); // 進行率計算に使用する演出時間
    const float progress = elapsedTime / safeDuration; // clamp前の演出進行率
    return (std::clamp)(progress, kEffectProgressMin, kEffectProgressMax);
}

/// <summary>
/// 設定値を0以上のパーティクル発生数へ補正する
/// </summary>
uint32_t ClampParticleCount(int count)
{
    const int safeCount = (std::max)(count, 0); // 負数を発生数として扱わないための補正値
    return static_cast<uint32_t>(safeCount);
}
}

/// <summary>
/// エフェクトの進行状態を初期状態へ戻す
/// </summary>
void TimeStopEffect::ResetState()
{
    phase_ = TimeStopPhase::Idle;
    phaseTime_ = kEffectTimeStart;
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
    postProcess.SetDistortionStrength(kEffectProgressMin);
    postProcess.SetDistortionProgress(kEffectProgressMin);
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
        postProcess.SetDistortionStrength(kEffectProgressMin);
    }
}

/// <summary>
/// 再開フェーズのポストエフェクト状態を反映する。
/// </summary>
void TimeStopEffect::ApplyReleasingPostProcess(PostProcess& postProcess, float progress)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetDistortionStrength(settings_.distortionStrength * (kEffectProgressMax - progress));
    postProcess.SetDistortionProgress(progress);
}

/// <summary>
/// 時間停止エフェクトを開始する
/// </summary>
void TimeStopEffect::Start(PostProcess& postProcess, const Vector2& effectCenter)
{
    previousPostEffect_ = postProcess.GetEffectType(); // 再生前のポストエフェクト
    phase_ = TimeStopPhase::Entering;
    phaseTime_ = kEffectTimeStart;

    ConfigureDistortionPostProcess(postProcess, effectCenter);

    ParticleManager* particleManager = ParticleManager::GetInstance(); // 開始演出を発生させるパーティクル管理
    if (particleManager) {
        particleManager->EmitCylinderEffect(kCylinderParticleGroupName, settings_.effectPosition, kStartCylinderCount);
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
    const auto transitionToPhase = [this](TimeStopPhase nextPhase) {
        phase_ = nextPhase;
        phaseTime_ = kEffectTimeStart;
    }; // フェーズ変更時に経過時間を初期化する処理

    switch (phase_) {
    case TimeStopPhase::Idle:
        break;

    case TimeStopPhase::Entering: {
        const float progress = CalculateEffectProgress(phaseTime_, settings_.enterDuration); // 開始演出の進行度
        ApplyEnteringPostProcess(postProcess, progress);

        if (progress >= kEffectProgressMax) {
            transitionToPhase(TimeStopPhase::Stopped);
            ApplyStoppedPostProcess(postProcess, true);
            if (particleManager) {
                particleManager->EmitRingEffect(
                    kRingParticleGroupName,
                    settings_.effectPosition,
                    ClampParticleCount(settings_.startRingCount));
            }
        }
        break;
    }

    case TimeStopPhase::Stopped:
        ApplyStoppedPostProcess(postProcess, false);
        if (phaseTime_ >= settings_.stopDuration) {
            transitionToPhase(TimeStopPhase::Releasing);
            if (particleManager) {
                particleManager->EmitRingEffect(
                    kRingParticleGroupName,
                    settings_.effectPosition,
                    ClampParticleCount(settings_.releaseRingCount));
                particleManager->EmitHitEffect(
                    kHitParticleGroupName,
                    settings_.effectPosition,
                    ClampParticleCount(settings_.releaseFragmentCount));
            }
        }
        break;

    case TimeStopPhase::Releasing: {
        const float progress = CalculateEffectProgress(phaseTime_, settings_.releaseDuration); // 再開演出の進行度
        ApplyReleasingPostProcess(postProcess, progress);

        if (progress >= kEffectProgressMax) {
            transitionToPhase(TimeStopPhase::Idle);
            postProcess.SetDistortionStrength(kEffectProgressMin);
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
        ImGui::DragFloat("Enter Duration", &settings_.enterDuration, kImGuiShortDurationStep, kImGuiMinimumShortDuration, kImGuiMaximumShortDuration, "%.2f sec");
        ImGui::DragFloat("Stop Duration", &settings_.stopDuration, kImGuiStopDurationStep, kImGuiMinimumStopDuration, kImGuiMaximumStopDuration, "%.2f sec");
        ImGui::DragFloat("Release Duration", &settings_.releaseDuration, kImGuiShortDurationStep, kImGuiMinimumShortDuration, kImGuiMaximumShortDuration, "%.2f sec");
        ImGui::SeparatorText("Distortion");
        ImGui::DragFloat("Stop Distortion Strength", &settings_.distortionStrength, kImGuiDistortionStrengthStep, kImGuiDistortionStrengthMin, kImGuiDistortionStrengthMax, "%.3f");
        ImGui::DragFloat("Stop Distortion Radius", &settings_.distortionRadius, kImGuiDistortionRadiusStep, kImGuiDistortionRadiusMin, kImGuiDistortionRadiusMax, "%.2f");
        ImGui::DragFloat("Stop Distortion Waves", &settings_.distortionWaveCount, kImGuiDistortionWaveStep, kImGuiDistortionWaveMin, kImGuiDistortionWaveMax, "%.1f");
        ImGui::SeparatorText("Particles");
        ImGui::SliderInt("Start Ring Count", &settings_.startRingCount, kImGuiRingCountMin, kImGuiRingCountMax);
        ImGui::SliderInt("Release Ring Count", &settings_.releaseRingCount, kImGuiRingCountMin, kImGuiRingCountMax);
        ImGui::SliderInt("Release Fragment Count", &settings_.releaseFragmentCount, kImGuiFragmentCountMin, kImGuiFragmentCountMax);
        ImGui::DragFloat3("Time Stop Position", &settings_.effectPosition.x, kImGuiPositionStep);
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
