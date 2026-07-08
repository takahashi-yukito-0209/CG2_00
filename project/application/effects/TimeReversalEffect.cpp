#include "TimeReversalEffect.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/base/WinApp.h"
#include "../../engine/particle/ParticleManager.h"
#include "../../engine/utility/mathUtility.h"

#include <algorithm>
#include <cstdint>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr float kMinimumEffectDuration = 0.0001f; // 演出時間のゼロ除算を防ぐ最小値
constexpr float kHistorySampleRate = 60.0f; // 履歴時間をフレーム数へ変換する基準値
constexpr float kTimeReversalDistortionDampingRate = 0.35f; // 時間逆行中の歪み減衰率
constexpr float kParticleShrinkRate = 0.35f; // 時間経過による粒子縮小率
constexpr float kParticleAlphaFadeRate = 0.5f; // 時間経過による粒子透明度の減衰率
constexpr float kRewindAfterimageBaseSizeRate = 0.85f; // 巻き戻し残像サイズの基準倍率
constexpr float kRewindAfterimageSizeStep = 0.12f; // 巻き戻し残像ごとのサイズ減少量
constexpr float kConvergenceFlashStartScale = 0.25f; // 収束フラッシュの開始倍率
constexpr float kConvergenceFlashScaleRange = 0.75f; // 収束フラッシュの拡大幅
constexpr float kConvergenceFlashColorAlpha = 0.75f; // 収束フラッシュ色の基準アルファ
constexpr float kConvergenceFlashGreen = 0.92f; // 収束フラッシュ色の緑成分
constexpr float kDirectionRandomMin = -1.0f; // 粒子方向乱数の最小値
constexpr float kDirectionRandomMax = 1.0f; // 粒子方向乱数の最大値
constexpr float kDirectionZScale = 0.45f; // Z方向の拡散を抑える倍率
constexpr int kMinimumParticleCount = 1; // 発生させる最小粒子数
constexpr float kMinimumTransformHistoryCount = 2.0f; // 補間に必要な最低履歴数
constexpr size_t kMaximumRewindAfterimageCount = 3; // 1粒子ごとの最大残像数
constexpr size_t kTimeReversalTargetIndex = 4; // Transformを巻き戻す対象番号
}

/// <summary>
/// エフェクトの進行状態を初期状態へ戻す
/// </summary>
void TimeReversalEffect::ResetState()
{
    phase_ = TimeReversalPhase::Idle;
    phaseTime_ = 0.0f;
    previousPostEffect_ = PostEffectType::Copy;
    particles_.clear();
    transformHistory_.clear();
}

/// <summary>
/// 調整値を初期値へ戻す
/// </summary>
void TimeReversalEffect::ResetSettings()
{
    settings_ = TimeReversalSettings {};
}

/// <summary>
/// 時間逆行開始時のポストエフェクト状態を設定する。
/// </summary>
void TimeReversalEffect::ConfigureInitialPostProcess(PostProcess& postProcess)
{
    postProcess.SetEffectType(PostEffectType::Copy);
    postProcess.SetDistortionRadius(settings_.distortionRadius);
    postProcess.SetDistortionWaveCount(settings_.distortionWaveCount);
    postProcess.SetDistortionStrength(0.0f);
    postProcess.SetDistortionProgress(0.0f);
}

/// <summary>
/// 巻き戻しフェーズのポストエフェクト状態を反映する。
/// </summary>
void TimeReversalEffect::ApplyRewindingPostProcess(PostProcess& postProcess, float rewindRate)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetDistortionRadius(settings_.distortionRadius);
    postProcess.SetDistortionWaveCount(settings_.distortionWaveCount);
    postProcess.SetDistortionStrength(settings_.distortionStrength * (1.0f - rewindRate * kTimeReversalDistortionDampingRate));
    postProcess.SetDistortionProgress(rewindRate);
}

/// <summary>
/// 収束フェーズのポストエフェクト状態を反映する。
/// </summary>
void TimeReversalEffect::ApplyConvergingPostProcess(PostProcess& postProcess, float convergenceRate)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetDistortionRadius(settings_.distortionRadius);
    postProcess.SetDistortionWaveCount(settings_.distortionWaveCount);
    postProcess.SetDistortionStrength(-settings_.distortionStrength * (1.0f - convergenceRate));
    postProcess.SetDistortionProgress(convergenceRate);
}

/// <summary>
/// 再生前のポストエフェクト状態へ戻す。
/// </summary>
void TimeReversalEffect::RestorePreviousPostProcess(PostProcess& postProcess)
{
    postProcess.SetEffectType(previousPostEffect_);
    postProcess.SetDistortionStrength(0.0f);
}

/// <summary>
/// 時間逆行エフェクトを開始する
/// </summary>
void TimeReversalEffect::Start(PostProcess& postProcess, size_t maximumSpriteCount)
{
    previousPostEffect_ = postProcess.GetEffectType(); // 再生前のポストエフェクト
    phase_ = TimeReversalPhase::Expanding;
    phaseTime_ = 0.0f;
    particles_.clear();

    const int particleCount = (std::clamp)(
        settings_.particleCount,
        1,
        static_cast<int>(maximumSpriteCount)); // 実際に発生する粒子数
    const float minimumSpeed = (std::min)(settings_.minSpeed, settings_.maxSpeed); // 発生速度の最小値
    const float maximumSpeed = (std::max)(settings_.minSpeed, settings_.maxSpeed); // 発生速度の最大値
    std::uniform_real_distribution<float> directionDistribution(kDirectionRandomMin, kDirectionRandomMax); // 方向成分の乱数
    std::uniform_real_distribution<float> speedDistribution(minimumSpeed, maximumSpeed); // 拡散速度の乱数

    particles_.reserve(static_cast<size_t>(particleCount));
    for (int particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
        Vector3 direction = {
            directionDistribution(random_),
            directionDistribution(random_),
            directionDistribution(random_) * kDirectionZScale,
        }; // 放射状に飛ばすための仮方向
        direction = MathUtil::Normalize(direction);
        const float speed = speedDistribution(random_); // 現在の粒子へ与える速度

        TimeReversalParticle particle {}; // 生成する時間逆行用パーティクル
        particle.position = settings_.effectPosition;
        particle.velocity = {
            direction.x * speed,
            direction.y * speed,
            direction.z * speed,
        };
        particle.rewindStartPosition = particle.position;
        particle.elapsedTime = 0.0f;
        particle.lifeTime = settings_.expansionDuration;
        particles_.push_back(particle);
    }

    ConfigureInitialPostProcess(postProcess);
}

/// <summary>
/// 時間逆行エフェクトの状態を更新する
/// </summary>
void TimeReversalEffect::Update(float deltaTime, PostProcess& postProcess, std::vector<std::unique_ptr<Object3d>>& objects3d)
{
    if (phase_ == TimeReversalPhase::Idle) {
        return;
    }

    phaseTime_ += deltaTime;

    if (phase_ == TimeReversalPhase::Paused) {
        if (phaseTime_ >= settings_.pauseDuration) {
            phase_ = TimeReversalPhase::Rewinding;
            phaseTime_ = 0.0f;
            for (TimeReversalParticle& particle : particles_) {
                particle.rewindStartPosition = particle.position;
            }
        }
        return;
    }

    if (phase_ == TimeReversalPhase::Rewinding) {
        const float rewindDuration = (std::max)(settings_.rewindDuration, kMinimumEffectDuration); // 巻き戻し時間
        const float rewindRate = (std::clamp)(phaseTime_ / rewindDuration, 0.0f, 1.0f); // 巻き戻しの進行度

        for (TimeReversalParticle& particle : particles_) {
            particle.position = {
                particle.rewindStartPosition.x + (settings_.effectPosition.x - particle.rewindStartPosition.x) * rewindRate,
                particle.rewindStartPosition.y + (settings_.effectPosition.y - particle.rewindStartPosition.y) * rewindRate,
                particle.rewindStartPosition.z + (settings_.effectPosition.z - particle.rewindStartPosition.z) * rewindRate,
            };
            particle.elapsedTime = particle.lifeTime * (1.0f - rewindRate);
        }
        ApplyRewindingPostProcess(postProcess, rewindRate);
        ApplyTransform(objects3d);

        if (rewindRate >= 1.0f) {
            particles_.clear();
            StartConvergence();
        }
        return;
    }

    if (phase_ == TimeReversalPhase::Converging) {
        const float convergenceDuration = (std::max)(settings_.convergenceDuration, kMinimumEffectDuration); // 収束演出時間
        const float convergenceRate = (std::clamp)(phaseTime_ / convergenceDuration, 0.0f, 1.0f); // 収束演出の進行度
        ApplyConvergingPostProcess(postProcess, convergenceRate);

        if (convergenceRate >= 1.0f) {
            phase_ = TimeReversalPhase::Idle;
            phaseTime_ = 0.0f;
            transformHistory_.clear();
            RestorePreviousPostProcess(postProcess);
        }
        return;
    }

    bool hasActiveParticle = false; // 拡散中の粒子が残っているか
    for (TimeReversalParticle& particle : particles_) {
        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;
        particle.elapsedTime += deltaTime;
        if (particle.elapsedTime < particle.lifeTime) {
            hasActiveParticle = true;
        }
    }

    if (!hasActiveParticle) {
        phase_ = TimeReversalPhase::Paused;
        phaseTime_ = 0.0f;
    }
}

/// <summary>
/// 時間逆行用スプライトを更新する
/// </summary>
void TimeReversalEffect::UpdateSprites(
    std::vector<std::unique_ptr<Sprite>>& particleSprites,
    std::vector<std::unique_ptr<Sprite>>& afterimageSprites,
    Sprite* convergenceSprite,
    const ScreenUvResolver& screenUvResolver)
{
    for (size_t spriteIndex = 0; spriteIndex < particleSprites.size(); ++spriteIndex) {
        Sprite* particleSprite = particleSprites[spriteIndex].get(); // 更新対象の粒子スプライト
        if (!particleSprite) {
            continue;
        }

        if (spriteIndex >= particles_.size() || phase_ == TimeReversalPhase::Idle) {
            particleSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            particleSprite->Update();
            continue;
        }

        const TimeReversalParticle& particle = particles_[spriteIndex]; // 表示する専用パーティクル
        const Vector2 screenUv = screenUvResolver(particle.position); // 粒子の画面UV
        const float lifeRate = particle.lifeTime > 0.0f
            ? (std::clamp)(particle.elapsedTime / particle.lifeTime, 0.0f, 1.0f)
            : 1.0f; // 拡散フェーズの進行度
        const float size = settings_.particleSize * (1.0f - lifeRate * kParticleShrinkRate); // 粒子サイズ

        particleSprite->SetPosition({
            screenUv.x * static_cast<float>(WinApp::kWindowWidth),
            screenUv.y * static_cast<float>(WinApp::kWindowHeight),
        });
        particleSprite->SetSize({ size, size });
        particleSprite->SetColor({
            settings_.particleColor.x,
            settings_.particleColor.y,
            settings_.particleColor.z,
            settings_.particleColor.w * (1.0f - lifeRate * kParticleAlphaFadeRate),
        });
        particleSprite->Update();
    }

    const size_t afterimageCount = static_cast<size_t>((std::clamp)(
        settings_.rewindAfterimageCount,
        0,
        static_cast<int>(kMaximumRewindAfterimageCount))); // 実際に表示する残像数
    const float rewindDuration = (std::max)(settings_.rewindDuration, kMinimumEffectDuration); // 残像位置計算用の巻き戻し時間
    const float rewindRate = (std::clamp)(phaseTime_ / rewindDuration, 0.0f, 1.0f); // 巻き戻しの進行度

    for (size_t spriteIndex = 0; spriteIndex < afterimageSprites.size(); ++spriteIndex) {
        Sprite* afterimageSprite = afterimageSprites[spriteIndex].get(); // 更新対象の残像スプライト
        if (!afterimageSprite) {
            continue;
        }

        const size_t particleIndex = spriteIndex / kMaximumRewindAfterimageCount; // 対応する粒子番号
        const size_t afterimageIndex = spriteIndex % kMaximumRewindAfterimageCount; // 粒子内での残像番号
        if (phase_ != TimeReversalPhase::Rewinding
            || particleIndex >= particles_.size()
            || afterimageIndex >= afterimageCount) {
            afterimageSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            afterimageSprite->Update();
            continue;
        }

        const TimeReversalParticle& particle = particles_[particleIndex]; // 残像元の粒子
        const float trailOffset = settings_.rewindAfterimageSpacing
            * static_cast<float>(afterimageIndex + 1)
            * (1.0f - rewindRate); // 軌跡上の遅れ
        const float afterimageRate = (std::clamp)(rewindRate - trailOffset, 0.0f, 1.0f); // 残像位置の進行度
        const Vector3 afterimagePosition = {
            particle.rewindStartPosition.x + (settings_.effectPosition.x - particle.rewindStartPosition.x) * afterimageRate,
            particle.rewindStartPosition.y + (settings_.effectPosition.y - particle.rewindStartPosition.y) * afterimageRate,
            particle.rewindStartPosition.z + (settings_.effectPosition.z - particle.rewindStartPosition.z) * afterimageRate,
        }; // 軌跡上の残像ワールド座標
        const Vector2 screenUv = screenUvResolver(afterimagePosition); // 残像の画面UV
        const float alphaRate = 1.0f
            - static_cast<float>(afterimageIndex) / static_cast<float>((std::max)(afterimageCount, size_t { 1 })); // 後方ほど薄くする係数
        const float sizeRate = kRewindAfterimageBaseSizeRate
            - static_cast<float>(afterimageIndex) * kRewindAfterimageSizeStep; // 後方ほど小さくする係数

        afterimageSprite->SetPosition({
            screenUv.x * static_cast<float>(WinApp::kWindowWidth),
            screenUv.y * static_cast<float>(WinApp::kWindowHeight),
        });
        afterimageSprite->SetSize({
            settings_.particleSize * sizeRate,
            settings_.particleSize * sizeRate,
        });
        afterimageSprite->SetColor({
            settings_.particleColor.x,
            settings_.particleColor.y,
            settings_.particleColor.z,
            settings_.particleColor.w * settings_.rewindAfterimageAlpha * alphaRate,
        });
        afterimageSprite->Update();
    }

    if (convergenceSprite) {
        if (phase_ != TimeReversalPhase::Converging) {
            convergenceSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            convergenceSprite->Update();
        } else {
            const float convergenceDuration = (std::max)(settings_.convergenceDuration, kMinimumEffectDuration); // フラッシュ計算用の収束時間
            const float convergenceRate = (std::clamp)(phaseTime_ / convergenceDuration, 0.0f, 1.0f); // フラッシュの進行度
            const Vector2 screenUv = screenUvResolver(settings_.effectPosition); // 収束地点の画面UV
            const float flashSize = settings_.convergenceFlashSize
                * (kConvergenceFlashStartScale + convergenceRate * kConvergenceFlashScaleRange); // 中心から広がるフラッシュサイズ
            const float flashAlpha = settings_.convergenceFlashAlpha * (1.0f - convergenceRate); // 終了へ向けて減衰する透明度

            convergenceSprite->SetPosition({
                screenUv.x * static_cast<float>(WinApp::kWindowWidth),
                screenUv.y * static_cast<float>(WinApp::kWindowHeight),
            });
            convergenceSprite->SetSize({ flashSize, flashSize });
            convergenceSprite->SetColor({
                kConvergenceFlashColorAlpha,
                kConvergenceFlashGreen,
                1.0f,
                flashAlpha,
            });
            convergenceSprite->Update();
        }
    }
}

/// <summary>
/// 巻き戻し対象のTransform履歴を更新する
/// </summary>
void TimeReversalEffect::UpdateTransformHistory(std::vector<std::unique_ptr<Object3d>>& objects3d, bool canRecordHistory)
{
    if (!canRecordHistory
        || phase_ != TimeReversalPhase::Idle
        || objects3d.size() <= kTimeReversalTargetIndex
        || !objects3d[kTimeReversalTargetIndex]) {
        return;
    }

    Object3d* timeReversalTarget = objects3d[kTimeReversalTargetIndex].get(); // Transform履歴を保存する対象
    Transform currentTransform {}; // 現在フレームのTransform
    currentTransform.scale = timeReversalTarget->GetScale();
    currentTransform.rotate = timeReversalTarget->GetRotate();
    currentTransform.translate = timeReversalTarget->GetTranslate();
    transformHistory_.push_front(currentTransform);

    const size_t maximumHistoryCount = static_cast<size_t>((std::max)(
        settings_.transformHistoryDuration * kHistorySampleRate,
        kMinimumTransformHistoryCount)); // 保持する最大Transform数
    while (transformHistory_.size() > maximumHistoryCount) {
        transformHistory_.pop_back();
    }
}

/// <summary>
/// 時間逆行用スプライトを描画する
/// </summary>
void TimeReversalEffect::DrawParticles(
    SpriteCommon* spriteCommon,
    std::vector<std::unique_ptr<Sprite>>& particleSprites,
    std::vector<std::unique_ptr<Sprite>>& afterimageSprites,
    Sprite* convergenceSprite)
{
    if (!spriteCommon || phase_ == TimeReversalPhase::Idle) {
        return;
    }

    spriteCommon->SetCommonDrawSetting();
    for (auto& afterimageSprite : afterimageSprites) {
        if (afterimageSprite && afterimageSprite->GetColor().w > 0.0f) {
            afterimageSprite->Draw();
        }
    }
    for (auto& particleSprite : particleSprites) {
        if (particleSprite && particleSprite->GetColor().w > 0.0f) {
            particleSprite->Draw();
        }
    }
    if (convergenceSprite && convergenceSprite->GetColor().w > 0.0f) {
        convergenceSprite->Draw();
    }
}

/// <summary>
/// ImGuiで時間逆行エフェクトの状態と調整値を表示する
/// </summary>
void TimeReversalEffect::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Time Reversal Phase: %s", GetPhaseName());
    ImGui::Text("Phase Time: %.3f", phaseTime_);

    if (ImGui::CollapsingHeader("Time Reversal Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Particle Count", &settings_.particleCount, 1, 128);
        ImGui::DragFloat("Min Speed", &settings_.minSpeed, 0.1f, 0.0f, 20.0f, "%.1f");
        ImGui::DragFloat("Max Speed", &settings_.maxSpeed, 0.1f, 0.0f, 20.0f, "%.1f");
        ImGui::DragFloat("Expansion Duration", &settings_.expansionDuration, 0.01f, 0.05f, 5.0f, "%.2f sec");
        ImGui::DragFloat("Pause Duration", &settings_.pauseDuration, 0.01f, 0.0f, 5.0f, "%.2f sec");
        ImGui::DragFloat("Rewind Duration", &settings_.rewindDuration, 0.01f, 0.05f, 5.0f, "%.2f sec");
        ImGui::SliderInt("Rewind Afterimage Count", &settings_.rewindAfterimageCount, 0, 3);
        ImGui::DragFloat("Rewind Afterimage Spacing", &settings_.rewindAfterimageSpacing, 0.01f, 0.0f, 0.5f, "%.2f");
        ImGui::DragFloat("Rewind Afterimage Alpha", &settings_.rewindAfterimageAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::SeparatorText("Screen Distortion");
        ImGui::DragFloat("Rewind Distortion Strength", &settings_.distortionStrength, 0.001f, -0.2f, 0.2f, "%.3f");
        ImGui::DragFloat("Rewind Distortion Radius", &settings_.distortionRadius, 0.01f, 0.05f, 1.0f, "%.2f");
        ImGui::DragFloat("Rewind Distortion Waves", &settings_.distortionWaveCount, 0.1f, 1.0f, 12.0f, "%.1f");
        ImGui::SeparatorText("Convergence");
        ImGui::DragFloat("Convergence Duration", &settings_.convergenceDuration, 0.01f, 0.05f, 2.0f, "%.2f sec");
        ImGui::DragFloat("Convergence Flash Size", &settings_.convergenceFlashSize, 1.0f, 8.0f, 512.0f, "%.0f");
        ImGui::DragFloat("Convergence Flash Alpha", &settings_.convergenceFlashAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::SliderInt("Convergence Ring Count", &settings_.convergenceRingCount, 0, 8);
        ImGui::SeparatorText("Transform Rewind");
        ImGui::DragFloat("Transform History Duration", &settings_.transformHistoryDuration, 0.1f, 0.1f, 10.0f, "%.1f sec");
        ImGui::DragFloat("Particle Size", &settings_.particleSize, 1.0f, 2.0f, 128.0f, "%.0f");
        ImGui::ColorEdit4("Particle Color", &settings_.particleColor.x);
        ImGui::DragFloat3("Effect Position", &settings_.effectPosition.x, 0.05f);
    }
#endif
}

/// <summary>
/// 時間逆行エフェクトが再生中か確認する
/// </summary>
bool TimeReversalEffect::IsPlaying() const
{
    return phase_ != TimeReversalPhase::Idle;
}

/// <summary>
/// エフェクト発生位置を取得する
/// </summary>
const Vector3& TimeReversalEffect::GetEffectPosition() const
{
    return settings_.effectPosition;
}

/// <summary>
/// 現在のフェーズ名を取得する
/// </summary>
const char* TimeReversalEffect::GetPhaseName() const
{
    switch (phase_) {
    case TimeReversalPhase::Idle:
        return "Idle";
    case TimeReversalPhase::Expanding:
        return "Expanding";
    case TimeReversalPhase::Paused:
        return "Paused";
    case TimeReversalPhase::Rewinding:
        return "Rewinding";
    case TimeReversalPhase::Converging:
        return "Converging";
    }

    return "Unknown";
}

/// <summary>
/// Transform履歴を対象オブジェクトへ適用する
/// </summary>
void TimeReversalEffect::ApplyTransform(std::vector<std::unique_ptr<Object3d>>& objects3d)
{
    if (objects3d.size() <= kTimeReversalTargetIndex
        || !objects3d[kTimeReversalTargetIndex]
        || transformHistory_.empty()) {
        return;
    }

    const float rewindDuration = (std::max)(settings_.rewindDuration, kMinimumEffectDuration); // Transform巻き戻し時間
    const float rewindRate = (std::clamp)(phaseTime_ / rewindDuration, 0.0f, 1.0f); // Transform巻き戻し進行度
    const float historyPosition = rewindRate * static_cast<float>(transformHistory_.size() - 1); // 履歴内の参照位置
    const size_t currentHistoryIndex = static_cast<size_t>(historyPosition); // 補間元の履歴番号
    const size_t nextHistoryIndex = (std::min)(currentHistoryIndex + 1, transformHistory_.size() - 1); // 補間先の履歴番号
    const float interpolationRate = historyPosition - static_cast<float>(currentHistoryIndex); // 履歴間の補間率
    const Transform& currentTransform = transformHistory_[currentHistoryIndex]; // 補間元Transform
    const Transform& nextTransform = transformHistory_[nextHistoryIndex]; // 補間先Transform

    const auto interpolateVector3 = [interpolationRate](const Vector3& start, const Vector3& end) {
        return Vector3 {
            start.x + (end.x - start.x) * interpolationRate,
            start.y + (end.y - start.y) * interpolationRate,
            start.z + (end.z - start.z) * interpolationRate,
        };
    }; // Transform要素を線形補間する処理

    Object3d* timeReversalTarget = objects3d[kTimeReversalTargetIndex].get(); // Transformを適用する対象
    timeReversalTarget->SetScale(interpolateVector3(currentTransform.scale, nextTransform.scale));
    timeReversalTarget->SetRotate(interpolateVector3(currentTransform.rotate, nextTransform.rotate));
    timeReversalTarget->SetTranslate(interpolateVector3(currentTransform.translate, nextTransform.translate));
}

/// <summary>
/// 収束フェーズを開始する
/// </summary>
void TimeReversalEffect::StartConvergence()
{
    phase_ = TimeReversalPhase::Converging;
    phaseTime_ = 0.0f;

    ParticleManager* particleManager = ParticleManager::GetInstance(); // 収束リングを生成するパーティクル管理
    if (particleManager && settings_.convergenceRingCount > 0) {
        particleManager->EmitRingEffect(
            "Ring",
            settings_.effectPosition,
            static_cast<uint32_t>(settings_.convergenceRingCount));
    }
}
