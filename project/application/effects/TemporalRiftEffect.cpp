#include "TemporalRiftEffect.h"
#include "EffectProgress.h"
#include "EffectParticleUtility.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/base/WinApp.h"
#include "../../engine/particle/ParticleManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

using namespace Math;
using namespace MyEngine;

namespace {
using EffectProgress::CalculateEffectProgress;
using EffectProgress::CalculateEffectRemainingProgress;
using EffectProgress::GetSafeEffectDuration;
using EffectProgress::kEffectProgressMax;
using EffectProgress::kEffectProgressMin;
using EffectProgress::kEffectTimeStart;
constexpr float kTemporalVerticalDisplacementRate = 0.25f; // 時間ずれの縦方向移動率
constexpr float kAfterimageBaseScaleRate = 0.75f; // 残像サイズの基準倍率
constexpr float kAfterimageScaleRange = 0.25f; // 残像サイズの変化幅
constexpr float kCrackRedBoost = 0.18f; // 亀裂色の赤成分加算量
constexpr float kCrackBlueBoost = 0.12f; // 亀裂色の青成分加算量
constexpr float kInnerRingStartRadius = 0.12f; // 内側リングの開始半径
constexpr float kInnerRingEndRadius = 1.8f; // 内側リングの終了半径
constexpr float kInnerRingLifeTime = 0.55f; // 内側リングの生存時間
constexpr float kOuterRingStartRadius = 0.28f; // 外側リングの開始半径
constexpr float kOuterRingEndRadius = 2.5f; // 外側リングの終了半径
constexpr float kOuterRingLifeTime = 0.85f; // 外側リングの生存時間
constexpr float kCameraShakePhaseX = 1.37f; // カメラ揺れX方向の位相倍率
constexpr float kCameraShakePhaseY = 1.91f; // カメラ揺れY方向の位相倍率
constexpr float kCameraShakePhaseZ = 1.13f; // カメラ揺れZ方向の位相倍率
constexpr float kCameraShakeOffsetY = 1.2f; // カメラ揺れY方向の位相オフセット
constexpr float kCameraShakeOffsetZ = 2.4f; // カメラ揺れZ方向の位相オフセット
constexpr float kCameraShakeAmplitudeY = 0.65f; // カメラ揺れY方向の振幅倍率
constexpr float kCameraShakeAmplitudeZ = 0.35f; // カメラ揺れZ方向の振幅倍率
constexpr float kTransformScaleComponentCount = 3.0f; // スケール平均に使用する成分数
constexpr float kMinimumAfterimageScale = 0.1f; // 残像表示に使用する最小スケール
constexpr float kPostEffectStrengthOff = 0.0f; // ポストエフェクト強度を無効化する値
constexpr Vector3 kDefaultEffectPosition = { 0.0f, 1.0f, 0.0f }; // エフェクト発生位置の初期値
constexpr Vector4 kHiddenAfterimageColor = { 0.0f, 0.0f, 0.0f, 0.0f }; // 非表示時の残像色
constexpr int kCrackRevealCountOffset = 1; // 進行率から亀裂表示数へ変換する加算数
constexpr int kBranchRateDenominatorMin = 1; // 枝分かれ率計算に使用する分母の最小値
constexpr int kAfterimageCurrentFrameCount = 1; // 残像履歴に現在フレーム分を含める数
constexpr int kAfterimageVisibleCountNone = 0; // 非表示時の残像表示数
constexpr int kAfterimageLifeRateDenominatorMin = 1; // 残像の新しさ計算に使用する分母の最小値
constexpr int kMinimumCrackEmitCount = 1; // 亀裂を発生させる最小数
constexpr int kMinimumBurstEmitCount = 0; // 破砕時に発生させる数の最小値
constexpr uint32_t kInnerRingCountBias = 1u; // 奇数リング数を内側へ寄せる加算値
constexpr const char* kEffectOwnerName = "TemporalRiftEffect"; // GPUプリセットログに使う演出名
constexpr const char* kRingParticleGroupName = "Ring"; // リング用パーティクルグループ名
constexpr const char* kHitParticleGroupName = "Hit"; // 亀裂と破片用パーティクルグループ名
constexpr const char* kCylinderParticleGroupName = "Cylinder"; // 円柱用パーティクルグループ名
constexpr const char* kGpuConeRisePresetName = "Cone Rise"; // 圧縮予兆に使うGPUプリセット名
constexpr const char* kGpuSparkBurstPresetName = "Spark Burst"; // 破砕衝撃に使うGPUプリセット名
constexpr uint32_t kStartCylinderCount = 1; // 開始時に発生させる円柱エフェクト数
constexpr int kMaximumCrackPatternCount = 7; // 定義済みの亀裂パターン数
constexpr std::array<float, kMaximumCrackPatternCount> kCrackAngles = {
    -1.15f, -0.72f, -0.28f, 0.12f, 0.55f, 0.95f, 1.34f,
}; // 各亀裂のZ軸回転
constexpr std::array<Vector3, kMaximumCrackPatternCount> kCrackOffsets = {
    Vector3 { -0.20f, 0.08f, 0.00f },
    Vector3 { -0.38f, -0.12f, 0.01f },
    Vector3 { -0.12f, 0.30f, 0.02f },
    Vector3 { 0.10f, -0.28f, 0.03f },
    Vector3 { 0.30f, 0.16f, 0.04f },
    Vector3 { 0.48f, -0.06f, 0.05f },
    Vector3 { 0.62f, 0.30f, 0.06f },
}; // 中心から少しずらして見せる位置補正
constexpr int kCrackWidthVariationCycle = 2; // 亀裂幅の変化周期
constexpr uint32_t kRingColorGroupCount = 2u; // 内側と外側に分けるリング色グループ数

constexpr size_t kTemporalTargetIndex = 4; // 時間ずれと残像の対象にする要素番号
constexpr float kImGuiDurationStep = 0.01f; // 時間設定の調整幅
constexpr float kImGuiDurationMin = 0.01f; // 時間設定の最小値
constexpr float kImGuiDurationMax = 2.0f; // 時間設定の最大値
constexpr float kImGuiBlurStep = 0.001f; // ブラー強度の調整幅
constexpr float kImGuiBlurMin = 0.0f; // ブラー強度の最小値
constexpr float kImGuiBlurMax = 0.1f; // ブラー強度の最大値
constexpr int kImGuiBlurSampleCountMin = 1; // ブラーサンプル数の最小値
constexpr int kImGuiBlurSampleCountMax = 32; // ブラーサンプル数の最大値
constexpr float kImGuiDistortionRadiusStep = 0.01f; // 歪み範囲の調整幅
constexpr float kImGuiDistortionRadiusMin = 0.01f; // 歪み範囲の最小値
constexpr float kImGuiDistortionRadiusMax = 1.5f; // 歪み範囲の最大値
constexpr float kImGuiDistortionStrengthStep = 0.001f; // 歪み強度の調整幅
constexpr float kImGuiCompressDistortionMin = -0.1f; // 圧縮歪み強度の最小値
constexpr float kImGuiCompressDistortionMax = 0.0f; // 圧縮歪み強度の最大値
constexpr float kImGuiBurstDistortionMin = 0.0f; // 破砕歪み強度の最小値
constexpr float kImGuiBurstDistortionMax = 0.1f; // 破砕歪み強度の最大値
constexpr float kImGuiDistortionWaveStep = 0.1f; // 歪み波数の調整幅
constexpr float kImGuiDistortionWaveMin = 0.0f; // 歪み波数の最小値
constexpr float kImGuiDistortionWaveMax = 12.0f; // 歪み波数の最大値
constexpr int kImGuiAfterimageCountMin = 1; // 残像数の最小値
constexpr int kImGuiAfterimageCountMax = 8; // 残像数の最大値
constexpr int kImGuiAfterimageIntervalMin = 1; // 残像履歴間隔の最小値
constexpr int kImGuiAfterimageIntervalMax = 12; // 残像履歴間隔の最大値
constexpr float kImGuiAfterimageSizeStep = 1.0f; // 残像サイズの調整幅
constexpr float kImGuiAfterimageSizeMin = 10.0f; // 残像サイズの最小値
constexpr float kImGuiAfterimageSizeMax = 300.0f; // 残像サイズの最大値
constexpr float kImGuiAlphaMin = 0.0f; // 透明度設定の最小値
constexpr float kImGuiAlphaMax = 1.0f; // 透明度設定の最大値
constexpr float kImGuiTemporalDisplacementStep = 0.05f; // 時間ずれ量の調整幅
constexpr float kImGuiTemporalDisplacementMin = 0.0f; // 時間ずれ量の最小値
constexpr float kImGuiTemporalDisplacementMax = 5.0f; // 時間ずれ量の最大値
constexpr float kImGuiHitStopDurationStep = 0.005f; // ヒットストップ時間の調整幅
constexpr float kImGuiHitStopDurationMin = 0.0f; // ヒットストップ時間の最小値
constexpr float kImGuiHitStopDurationMax = 0.5f; // ヒットストップ時間の最大値
constexpr float kImGuiCameraShakeDurationStep = 0.01f; // カメラ揺れ時間の調整幅
constexpr float kImGuiCameraShakeDurationMin = 0.0f; // カメラ揺れ時間の最小値
constexpr float kImGuiCameraShakeDurationMax = 2.0f; // カメラ揺れ時間の最大値
constexpr float kImGuiCameraShakeStrengthStep = 0.01f; // カメラ揺れ強度の調整幅
constexpr float kImGuiCameraShakeStrengthMin = 0.0f; // カメラ揺れ強度の最小値
constexpr float kImGuiCameraShakeStrengthMax = 2.0f; // カメラ揺れ強度の最大値
constexpr float kImGuiCameraShakeFrequencyStep = 1.0f; // カメラ揺れ周波数の調整幅
constexpr float kImGuiCameraShakeFrequencyMin = 1.0f; // カメラ揺れ周波数の最小値
constexpr float kImGuiCameraShakeFrequencyMax = 120.0f; // カメラ揺れ周波数の最大値
constexpr float kImGuiPositionStep = 0.05f; // 発生位置の調整幅
constexpr int kImGuiCrackCountMin = 1; // 亀裂数の最小値
constexpr int kImGuiCrackCountMax = 7; // 亀裂数の最大値
constexpr float kImGuiCrackLengthStep = 0.05f; // 亀裂長さの調整幅
constexpr float kImGuiCrackLengthMin = 0.05f; // 亀裂長さの最小値
constexpr float kImGuiCrackLengthMax = 10.0f; // 亀裂長さの最大値
constexpr float kImGuiCrackLengthVariationStep = 0.01f; // 亀裂長さばらつきの調整幅
constexpr float kImGuiCrackLengthVariationMin = 0.0f; // 亀裂長さばらつきの最小値
constexpr float kImGuiCrackLengthVariationMax = 3.0f; // 亀裂長さばらつきの最大値
constexpr float kImGuiCrackWidthStep = 0.005f; // 亀裂幅の調整幅
constexpr float kImGuiCrackWidthMin = 0.005f; // 亀裂幅の最小値
constexpr float kImGuiCrackWidthMax = 1.0f; // 亀裂幅の最大値
constexpr float kImGuiCrackWidthVariationMin = 0.0f; // 亀裂幅ばらつきの最小値
constexpr float kImGuiCrackWidthVariationMax = 1.0f; // 亀裂幅ばらつきの最大値
constexpr float kImGuiCrackLifeTimeMax = 5.0f; // 亀裂寿命の最大値
constexpr int kImGuiRingCountMin = 0; // リング数の最小値
constexpr int kImGuiRingCountMax = 8; // リング数の最大値
constexpr int kImGuiFragmentCountMin = 0; // 破片数の最小値
constexpr int kImGuiFragmentCountMax = 64; // 破片数の最大値
constexpr float kImGuiFragmentSpeedStep = 0.1f; // 破片速度の調整幅
constexpr float kImGuiFragmentSpeedMin = 0.0f; // 破片速度の最小値
constexpr float kImGuiFragmentSpeedMax = 20.0f; // 破片速度の最大値
constexpr float kImGuiFragmentLifeTimeStep = 0.01f; // 破片寿命の調整幅
constexpr float kImGuiFragmentLifeTimeMin = 0.05f; // 破片寿命の最小値
constexpr float kImGuiFragmentLifeTimeMax = 3.0f; // 破片寿命の最大値

}

/// <summary>
/// エフェクトの進行状態を初期状態へ戻す
/// </summary>
void TemporalRiftEffect::ResetState(Camera* camera)
{
    StopCameraShake(camera);
    phase_ = TemporalRiftPhase::Idle;
    phaseTime_ = kEffectTimeStart;
    previousPostEffect_ = PostEffectType::Copy;
    cracksEmitted_ = 0;
    transformHistory_.clear();
    hasTargetBaseTransform_ = false;
    hitStopRemainingTime_ = kEffectTimeStart;
}

/// <summary>
/// 調整値を初期値へ戻す
/// </summary>
void TemporalRiftEffect::ResetSettings()
{
    settings_ = TemporalRiftSettings {};
    effectPosition_ = kDefaultEffectPosition;
}

/// <summary>
/// 演出開始時のポストエフェクト状態を設定する。
/// </summary>
void TemporalRiftEffect::ConfigureInitialPostProcess(PostProcess& postProcess)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetRadialBlurCenter(screenUv_);
    postProcess.SetDistortionCenter(screenUv_);
    postProcess.SetDistortionRadius(settings_.distortionRadius);
    postProcess.SetDistortionWaveCount(settings_.distortionWaveCount);
    postProcess.SetDistortionStrength(kPostEffectStrengthOff);
    postProcess.SetDistortionProgress(kEffectProgressMin);
    postProcess.SetRadialBlurWidth(settings_.compressBlurStart);
    postProcess.SetRadialBlurSampleCount(static_cast<uint32_t>(settings_.blurSampleCount));
}

/// <summary>
/// 圧縮フェーズのポストエフェクト状態を反映する。
/// </summary>
void TemporalRiftEffect::ApplyCompressPostProcess(PostProcess& postProcess, float progress, float blurWidth)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetDistortionRadius(settings_.distortionRadius);
    postProcess.SetDistortionWaveCount(settings_.distortionWaveCount);
    postProcess.SetDistortionStrength(settings_.compressDistortionStrength * progress);
    postProcess.SetDistortionProgress(progress);
    postProcess.SetRadialBlurWidth(blurWidth);
}

/// <summary>
/// 停止表示フェーズのポストエフェクト状態を反映する。
/// </summary>
void TemporalRiftEffect::ApplyGrayscalePostProcess(PostProcess& postProcess)
{
    postProcess.SetEffectType(PostEffectType::Grayscale);
}

/// <summary>
/// 破砕フェーズのポストエフェクト状態を反映する。
/// </summary>
void TemporalRiftEffect::ApplyBurstPostProcess(PostProcess& postProcess, float progress)
{
    postProcess.SetEffectType(PostEffectType::Distortion);
    postProcess.SetDistortionRadius(settings_.distortionRadius);
    postProcess.SetDistortionWaveCount(settings_.distortionWaveCount);
    postProcess.SetDistortionStrength(settings_.burstDistortionStrength * (kEffectProgressMax - progress));
    postProcess.SetDistortionProgress(progress);
    postProcess.SetRadialBlurWidth(settings_.burstBlurStrength * (kEffectProgressMax - progress));
}

/// <summary>
/// 復帰フェーズのポストエフェクト状態を反映する。
/// </summary>
void TemporalRiftEffect::ApplyRecoverPostProcess(PostProcess& postProcess)
{
    postProcess.SetEffectType(PostEffectType::Copy);
}

/// <summary>
/// 再生前のポストエフェクト状態へ戻す。
/// </summary>
void TemporalRiftEffect::RestorePreviousPostProcess(PostProcess& postProcess)
{
    postProcess.SetEffectType(previousPostEffect_);
}

/// <summary>
/// 時空破砕エフェクトを開始する
/// </summary>
void TemporalRiftEffect::Start(PostProcess& postProcess, std::vector<std::unique_ptr<Object3d>>& objects3d, const Vector2& screenUv)
{
    previousPostEffect_ = postProcess.GetEffectType(); // 再生前のポストエフェクト
    screenUv_ = screenUv;

    if (objects3d.size() > kTemporalTargetIndex && objects3d[kTemporalTargetIndex]) {
        Object3d* temporalTarget = objects3d[kTemporalTargetIndex].get(); // 時間ずれ対象の3Dオブジェクト
        targetBaseTransform_.scale = temporalTarget->GetScale();
        targetBaseTransform_.rotate = temporalTarget->GetRotate();
        targetBaseTransform_.translate = temporalTarget->GetTranslate();
        hasTargetBaseTransform_ = true;
        transformHistory_.clear();
    }

    phase_ = TemporalRiftPhase::Compress;
    phaseTime_ = kEffectTimeStart;
    cracksEmitted_ = 0;
    ConfigureInitialPostProcess(postProcess);

    ParticleManager* particleManager = ParticleManager::GetInstance(); // 圧縮予兆を生成するパーティクル管理
    if (particleManager) {
        particleManager->EmitCylinderEffect(kCylinderParticleGroupName, effectPosition_, kStartCylinderCount);
        EffectParticleUtility::PlayGpuParticlePreset(kEffectOwnerName, kGpuConeRisePresetName, effectPosition_);
    }
}

/// <summary>
/// 時空破砕エフェクトの状態を更新する
/// </summary>
void TemporalRiftEffect::Update(float deltaTime, PostProcess& postProcess, Camera* camera)
{
    phaseTime_ += deltaTime;

    const auto transitionToPhase = [this](TemporalRiftPhase nextPhase) {
        phase_ = nextPhase;
        phaseTime_ = kEffectTimeStart;
    }; // フェーズ変更時に経過時間を初期化する処理

    switch (phase_) {
    case TemporalRiftPhase::Idle:
        break;

    case TemporalRiftPhase::Compress: {
        const float compressDuration = GetSafeEffectDuration(settings_.compressDuration); // 空間圧縮を見せる時間
        const float progress = CalculateEffectProgress(phaseTime_, settings_.compressDuration); // 圧縮演出の進行度
        const float blurWidth = settings_.compressBlurStart
            + (settings_.compressBlurEnd - settings_.compressBlurStart) * progress; // 現在のブラー幅
        ApplyCompressPostProcess(postProcess, progress, blurWidth);

        if (phaseTime_ >= compressDuration) {
            transitionToPhase(TemporalRiftPhase::Freeze);
        }
        break;
    }

    case TemporalRiftPhase::Freeze:
        ApplyGrayscalePostProcess(postProcess);
        if (phaseTime_ >= settings_.freezeDuration) {
            transitionToPhase(TemporalRiftPhase::Crack);
        }
        break;

    case TemporalRiftPhase::Crack:
        ApplyGrayscalePostProcess(postProcess);
        EmitSpaceCracks();
        if (phaseTime_ >= settings_.crackDuration) {
            transitionToPhase(TemporalRiftPhase::Burst);
            EmitRiftBurst();
            StartImpact(camera);
        }
        break;

    case TemporalRiftPhase::Burst: {
        const float burstDuration = GetSafeEffectDuration(settings_.burstDuration); // 破砕の衝撃を見せる時間
        const float progress = CalculateEffectProgress(phaseTime_, settings_.burstDuration); // 破砕演出の進行度
        ApplyBurstPostProcess(postProcess, progress);

        if (phaseTime_ >= burstDuration) {
            transitionToPhase(TemporalRiftPhase::Recover);
        }
        break;
    }

    case TemporalRiftPhase::Recover:
        ApplyRecoverPostProcess(postProcess);
        if (phaseTime_ >= settings_.recoverDuration) {
            transitionToPhase(TemporalRiftPhase::Idle);
            RestorePreviousPostProcess(postProcess);
        }
        break;
    }
}

/// <summary>
/// 時間ずれ対象のTransform履歴を更新する
/// </summary>
void TemporalRiftEffect::UpdateAfterimages(std::vector<std::unique_ptr<Object3d>>& objects3d)
{
    if (objects3d.size() <= kTemporalTargetIndex || !objects3d[kTemporalTargetIndex]) {
        return;
    }

    Object3d* temporalTarget = objects3d[kTemporalTargetIndex].get(); // 時間ずれ対象の3Dオブジェクト
    if (!hasTargetBaseTransform_) {
        return;
    }

    float displacementRate = kEffectProgressMin; // 現在フェーズで適用する時間ずれ移動率
    switch (phase_) {
    case TemporalRiftPhase::Compress:
        displacementRate = CalculateEffectProgress(phaseTime_, settings_.compressDuration);
        break;
    case TemporalRiftPhase::Freeze:
        displacementRate = kEffectProgressMax;
        break;
    case TemporalRiftPhase::Crack:
        displacementRate = CalculateEffectRemainingProgress(phaseTime_, settings_.crackDuration);
        break;
    case TemporalRiftPhase::Burst:
    case TemporalRiftPhase::Recover:
        displacementRate = kEffectProgressMin;
        break;
    case TemporalRiftPhase::Idle:
        temporalTarget->SetScale(targetBaseTransform_.scale);
        temporalTarget->SetRotate(targetBaseTransform_.rotate);
        temporalTarget->SetTranslate(targetBaseTransform_.translate);
        hasTargetBaseTransform_ = false;
        transformHistory_.clear();
        return;
    }

    const float displacement = settings_.temporalDisplacement * displacementRate; // 現在の時間ずれ移動量
    Vector3 displacedPosition = targetBaseTransform_.translate; // 時間ずれ適用後の位置
    displacedPosition.x += displacement;
    displacedPosition.y += displacement * kTemporalVerticalDisplacementRate;
    temporalTarget->SetTranslate(displacedPosition);

    Transform currentTransform {}; // 履歴へ保存する現在のTransform
    currentTransform.scale = temporalTarget->GetScale();
    currentTransform.rotate = temporalTarget->GetRotate();
    currentTransform.translate = temporalTarget->GetTranslate();
    transformHistory_.push_front(currentTransform);

    const size_t maximumHistoryCount = static_cast<size_t>(
        settings_.afterimageCount * settings_.afterimageFrameInterval + kAfterimageCurrentFrameCount); // 残像表示に必要な最大履歴数
    while (transformHistory_.size() > maximumHistoryCount) {
        transformHistory_.pop_back();
    }
}

/// <summary>
/// Transform履歴から残像スプライトを更新する
/// </summary>
void TemporalRiftEffect::UpdateAfterimageSprites(std::vector<std::unique_ptr<Sprite>>& afterimageSprites, const ScreenUvResolver& screenUvResolver)
{
    const int visibleAfterimageCount = phase_ == TemporalRiftPhase::Idle
        ? kAfterimageVisibleCountNone
        : (std::min)(settings_.afterimageCount, static_cast<int>(afterimageSprites.size())); // 実際に表示する残像数

    for (size_t spriteIndex = 0; spriteIndex < afterimageSprites.size(); ++spriteIndex) {
        Sprite* afterimageSprite = afterimageSprites[spriteIndex].get(); // 更新対象の残像スプライト
        if (!afterimageSprite) {
            continue;
        }

        const size_t historyIndex = spriteIndex * static_cast<size_t>(settings_.afterimageFrameInterval); // 残像が参照する履歴番号
        if (static_cast<int>(spriteIndex) >= visibleAfterimageCount || historyIndex >= transformHistory_.size()) {
            afterimageSprite->SetColor(kHiddenAfterimageColor);
            afterimageSprite->Update();
            continue;
        }

        const Transform& historyTransform = transformHistory_[historyIndex]; // 表示する過去Transform
        const Vector2 screenUv = screenUvResolver(historyTransform.translate); // 過去位置の画面UV
        const float lifeRate = kEffectProgressMax
            - static_cast<float>(spriteIndex) / static_cast<float>((std::max)(visibleAfterimageCount, kAfterimageLifeRateDenominatorMin)); // 残像の新しさ
        const float scaleAverage = (historyTransform.scale.x + historyTransform.scale.y + historyTransform.scale.z) / kTransformScaleComponentCount; // Transformのスケール平均
        const float spriteSize = settings_.afterimageSize
            * (std::max)(scaleAverage, kMinimumAfterimageScale)
            * (kAfterimageBaseScaleRate + lifeRate * kAfterimageScaleRange); // 過去Transformに応じた表示サイズ

        afterimageSprite->SetPosition({
            screenUv.x * static_cast<float>(WinApp::kWindowWidth),
            screenUv.y * static_cast<float>(WinApp::kWindowHeight),
        });
        afterimageSprite->SetRotation(historyTransform.rotate.z);
        afterimageSprite->SetSize({ spriteSize, spriteSize });
        afterimageSprite->SetColor({
            settings_.afterimageColor.x,
            settings_.afterimageColor.y,
            settings_.afterimageColor.z,
            settings_.afterimageAlpha * lifeRate,
        });
        afterimageSprite->Update();
    }
}

/// <summary>
/// Transform履歴による残像を描画する
/// </summary>
void TemporalRiftEffect::DrawAfterimages(SpriteCommon* spriteCommon, std::vector<std::unique_ptr<Sprite>>& afterimageSprites)
{
    if (!spriteCommon || phase_ == TemporalRiftPhase::Idle) {
        return;
    }

    spriteCommon->SetCommonDrawSetting();
    for (auto& afterimageSprite : afterimageSprites) {
        if (afterimageSprite && afterimageSprite->GetColor().w > kEffectProgressMin) {
            afterimageSprite->Draw();
        }
    }
}

/// <summary>
/// ヒットストップとカメラシェイクを更新する
/// </summary>
void TemporalRiftEffect::UpdateImpact(float deltaTime, Camera* camera)
{
    hitStopRemainingTime_ = (std::max)(hitStopRemainingTime_ - deltaTime, kEffectTimeStart);

    if (!isCameraShakeActive_ || !camera) {
        return;
    }

    cameraShakeRemainingTime_ = (std::max)(cameraShakeRemainingTime_ - deltaTime, kEffectTimeStart);
    cameraShakeElapsedTime_ += deltaTime;

    if (cameraShakeRemainingTime_ <= kEffectTimeStart || settings_.cameraShakeDuration <= kEffectTimeStart) {
        StopCameraShake(camera);
        return;
    }

    const float shakeRate = cameraShakeRemainingTime_ / settings_.cameraShakeDuration; // 揺れの残り割合
    const float phase = cameraShakeElapsedTime_ * settings_.cameraShakeFrequency; // 現在の振動位相
    const float strength = settings_.cameraShakeStrength * shakeRate; // 減衰を反映した現在の揺れ幅
    const Vector3 shakeOffset = {
        std::sin(phase * kCameraShakePhaseX) * strength,
        std::sin(phase * kCameraShakePhaseY + kCameraShakeOffsetY) * strength * kCameraShakeAmplitudeY,
        std::sin(phase * kCameraShakePhaseZ + kCameraShakeOffsetZ) * strength * kCameraShakeAmplitudeZ,
    }; // 軸ごとに周期をずらしたカメラ移動量
    camera->SetTranslate({
        cameraShakeBasePosition_.x + shakeOffset.x,
        cameraShakeBasePosition_.y + shakeOffset.y,
        cameraShakeBasePosition_.z + shakeOffset.z,
    });
}

/// <summary>
/// カメラシェイクを終了してカメラ位置を復元する
/// </summary>
void TemporalRiftEffect::StopCameraShake(Camera* camera)
{
    if (isCameraShakeActive_ && camera) {
        camera->SetTranslate(cameraShakeBasePosition_);
        camera->Update();
    }

    isCameraShakeActive_ = false;
    cameraShakeRemainingTime_ = kEffectTimeStart;
    cameraShakeElapsedTime_ = kEffectTimeStart;
}

/// <summary>
/// ImGuiで時空破砕エフェクトの状態と調整値を表示する
/// </summary>
void TemporalRiftEffect::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Rift Phase: %s", GetPhaseName());
    ImGui::Text("Phase Time: %.3f", phaseTime_);
    ImGui::Text("Blur Center UV: %.3f, %.3f", screenUv_.x, screenUv_.y);
    if (ImGui::Button("Reset Settings")) {
        ResetSettings();
    }

    if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Compress Duration", &settings_.compressDuration, kImGuiDurationStep, kImGuiDurationMin, kImGuiDurationMax, "%.2f sec");
        ImGui::DragFloat("Freeze Duration", &settings_.freezeDuration, kImGuiDurationStep, kImGuiDurationMin, kImGuiDurationMax, "%.2f sec");
        ImGui::DragFloat("Crack Duration", &settings_.crackDuration, kImGuiDurationStep, kImGuiDurationMin, kImGuiDurationMax, "%.2f sec");
        ImGui::DragFloat("Burst Duration", &settings_.burstDuration, kImGuiDurationStep, kImGuiDurationMin, kImGuiDurationMax, "%.2f sec");
        ImGui::DragFloat("Recover Duration", &settings_.recoverDuration, kImGuiDurationStep, kImGuiDurationMin, kImGuiDurationMax, "%.2f sec");
    }

    if (ImGui::CollapsingHeader("Radial Blur", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Compress Blur Start", &settings_.compressBlurStart, kImGuiBlurStep, kImGuiBlurMin, kImGuiBlurMax, "%.3f");
        ImGui::DragFloat("Compress Blur End", &settings_.compressBlurEnd, kImGuiBlurStep, kImGuiBlurMin, kImGuiBlurMax, "%.3f");
        ImGui::DragFloat("Burst Blur Strength", &settings_.burstBlurStrength, kImGuiBlurStep, kImGuiBlurMin, kImGuiBlurMax, "%.3f");
        ImGui::SliderInt("Blur Sample Count", &settings_.blurSampleCount, kImGuiBlurSampleCountMin, kImGuiBlurSampleCountMax);
    }

    if (ImGui::CollapsingHeader("Distortion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Distortion Radius", &settings_.distortionRadius, kImGuiDistortionRadiusStep, kImGuiDistortionRadiusMin, kImGuiDistortionRadiusMax, "%.2f");
        ImGui::DragFloat("Compress Distortion", &settings_.compressDistortionStrength, kImGuiDistortionStrengthStep, kImGuiCompressDistortionMin, kImGuiCompressDistortionMax, "%.3f");
        ImGui::DragFloat("Burst Distortion", &settings_.burstDistortionStrength, kImGuiDistortionStrengthStep, kImGuiBurstDistortionMin, kImGuiBurstDistortionMax, "%.3f");
        ImGui::DragFloat("Distortion Wave Count", &settings_.distortionWaveCount, kImGuiDistortionWaveStep, kImGuiDistortionWaveMin, kImGuiDistortionWaveMax, "%.1f");
    }

    if (ImGui::CollapsingHeader("Afterimage", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Afterimage Count", &settings_.afterimageCount, kImGuiAfterimageCountMin, kImGuiAfterimageCountMax);
        ImGui::SliderInt("History Interval", &settings_.afterimageFrameInterval, kImGuiAfterimageIntervalMin, kImGuiAfterimageIntervalMax);
        ImGui::DragFloat("Afterimage Size", &settings_.afterimageSize, kImGuiAfterimageSizeStep, kImGuiAfterimageSizeMin, kImGuiAfterimageSizeMax, "%.0f");
        ImGui::SliderFloat("Afterimage Alpha", &settings_.afterimageAlpha, kImGuiAlphaMin, kImGuiAlphaMax, "%.2f");
        ImGui::DragFloat("Temporal Displacement", &settings_.temporalDisplacement, kImGuiTemporalDisplacementStep, kImGuiTemporalDisplacementMin, kImGuiTemporalDisplacementMax, "%.2f");
        ImGui::ColorEdit3("Afterimage Color", &settings_.afterimageColor.x);
    }

    if (ImGui::CollapsingHeader("Impact", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Hit Stop Duration", &settings_.hitStopDuration, kImGuiHitStopDurationStep, kImGuiHitStopDurationMin, kImGuiHitStopDurationMax, "%.3f sec");
        ImGui::DragFloat("Camera Shake Duration", &settings_.cameraShakeDuration, kImGuiCameraShakeDurationStep, kImGuiCameraShakeDurationMin, kImGuiCameraShakeDurationMax, "%.2f sec");
        ImGui::DragFloat("Camera Shake Strength", &settings_.cameraShakeStrength, kImGuiCameraShakeStrengthStep, kImGuiCameraShakeStrengthMin, kImGuiCameraShakeStrengthMax, "%.2f");
        ImGui::DragFloat("Camera Shake Frequency", &settings_.cameraShakeFrequency, kImGuiCameraShakeFrequencyStep, kImGuiCameraShakeFrequencyMin, kImGuiCameraShakeFrequencyMax, "%.0f");
        ImGui::Text("Hit Stop Remaining: %.3f", hitStopRemainingTime_);
        ImGui::Text("Shake Remaining: %.3f", cameraShakeRemainingTime_);
    }

    if (ImGui::CollapsingHeader("Crack", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Effect Position", &effectPosition_.x, kImGuiPositionStep);
        ImGui::SliderInt("Crack Count", &settings_.crackCount, kImGuiCrackCountMin, kImGuiCrackCountMax);
        ImGui::DragFloat("Crack Length", &settings_.crackLength, kImGuiCrackLengthStep, kImGuiCrackLengthMin, kImGuiCrackLengthMax);
        ImGui::DragFloat("Length Variation", &settings_.crackLengthVariation, kImGuiCrackLengthVariationStep, kImGuiCrackLengthVariationMin, kImGuiCrackLengthVariationMax);
        ImGui::DragFloat("Crack Width", &settings_.crackWidth, kImGuiCrackWidthStep, kImGuiCrackWidthMin, kImGuiCrackWidthMax);
        ImGui::DragFloat("Width Variation", &settings_.crackWidthVariation, kImGuiCrackWidthStep, kImGuiCrackWidthVariationMin, kImGuiCrackWidthVariationMax);
        ImGui::DragFloat("Crack Life Time", &settings_.crackLifeTime, kImGuiDurationStep, kImGuiDurationMin, kImGuiCrackLifeTimeMax, "%.2f sec");
        ImGui::ColorEdit4("Crack Color", &settings_.crackColor.x);
    }

    if (ImGui::CollapsingHeader("Burst", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Ring Count", &settings_.ringCount, kImGuiRingCountMin, kImGuiRingCountMax);
        ImGui::SliderInt("Fragment Count", &settings_.fragmentCount, kImGuiFragmentCountMin, kImGuiFragmentCountMax);
        ImGui::SliderInt("GPU Failure Fragment Count", &settings_.gpuFailureFallbackFragmentCount, kImGuiFragmentCountMin, kImGuiFragmentCountMax);
        ImGui::ColorEdit4("Inner Ring Color", &settings_.innerRingColor.x);
        ImGui::ColorEdit4("Outer Ring Color", &settings_.outerRingColor.x);
        ImGui::ColorEdit4("Fragment Color", &settings_.fragmentColor.x);
        ImGui::DragFloat("Fragment Min Speed", &settings_.fragmentMinSpeed, kImGuiFragmentSpeedStep, kImGuiFragmentSpeedMin, kImGuiFragmentSpeedMax, "%.1f");
        ImGui::DragFloat("Fragment Max Speed", &settings_.fragmentMaxSpeed, kImGuiFragmentSpeedStep, kImGuiFragmentSpeedMin, kImGuiFragmentSpeedMax, "%.1f");
        ImGui::DragFloat("Fragment Life Time", &settings_.fragmentLifeTime, kImGuiFragmentLifeTimeStep, kImGuiFragmentLifeTimeMin, kImGuiFragmentLifeTimeMax, "%.2f sec");
    }
#endif
}

/// <summary>
/// 時空破砕エフェクトが再生中か確認する
/// </summary>
bool TemporalRiftEffect::IsPlaying() const
{
    return phase_ != TemporalRiftPhase::Idle;
}

/// <summary>
/// 歪みとブラーを連結するフェーズか確認する
/// </summary>
bool TemporalRiftEffect::IsPostProcessChainPhase() const
{
    return phase_ == TemporalRiftPhase::Compress || phase_ == TemporalRiftPhase::Burst;
}

/// <summary>
/// ヒットストップの残り時間を取得する
/// </summary>
float TemporalRiftEffect::GetHitStopRemainingTime() const
{
    return hitStopRemainingTime_;
}

/// <summary>
/// エフェクト発生位置を取得する
/// </summary>
const Vector3& TemporalRiftEffect::GetEffectPosition() const
{
    return effectPosition_;
}

/// <summary>
/// 画面UV座標を取得する
/// </summary>
const Vector2& TemporalRiftEffect::GetScreenUv() const
{
    return screenUv_;
}

/// <summary>
/// 画面UV座標を設定する
/// </summary>
void TemporalRiftEffect::SetScreenUv(const Vector2& screenUv)
{
    screenUv_ = screenUv;
}

/// <summary>
/// 現在のフェーズ名を取得する
/// </summary>
const char* TemporalRiftEffect::GetPhaseName() const
{
    switch (phase_) {
    case TemporalRiftPhase::Idle:
        return "Idle";
    case TemporalRiftPhase::Compress:
        return "Compress";
    case TemporalRiftPhase::Freeze:
        return "Freeze";
    case TemporalRiftPhase::Crack:
        return "Crack";
    case TemporalRiftPhase::Burst:
        return "Burst";
    case TemporalRiftPhase::Recover:
        return "Recover";
    }

    return "Unknown";
}

/// <summary>
/// 破砕時のヒットストップとカメラシェイクを開始する
/// </summary>
void TemporalRiftEffect::StartImpact(Camera* camera)
{
    hitStopRemainingTime_ = settings_.hitStopDuration;
    cameraShakeRemainingTime_ = settings_.cameraShakeDuration;
    cameraShakeElapsedTime_ = kEffectTimeStart;

    if (camera) {
        cameraShakeBasePosition_ = camera->GetTranslate();
        isCameraShakeActive_ = cameraShakeRemainingTime_ > kEffectTimeStart;
    }
}

/// <summary>
/// 空間亀裂を進行度に合わせて段階的に発生する
/// </summary>
void TemporalRiftEffect::EmitSpaceCracks()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 亀裂を発生するパーティクル管理
    if (!particleManager) {
        return;
    }


    const int crackCount = (std::clamp)(settings_.crackCount, kMinimumCrackEmitCount, static_cast<int>(kCrackAngles.size())); // 発生する亀裂総数
    const float progress = CalculateEffectProgress(phaseTime_, settings_.crackDuration); // 亀裂展開の進行度
    const int targetCrackCount = (std::min)(
        crackCount,
        static_cast<int>(progress * static_cast<float>(crackCount)) + kCrackRevealCountOffset); // 現在までに表示する亀裂数

    while (cracksEmitted_ < targetCrackCount) {
        const size_t crackIndex = static_cast<size_t>(cracksEmitted_); // 今回発生する亀裂番号
        const Vector3& offset = kCrackOffsets[crackIndex]; // 亀裂の位置補正
        const Vector3 crackPosition = {
            effectPosition_.x + offset.x,
            effectPosition_.y + offset.y,
            effectPosition_.z + offset.z,
        }; // 亀裂のワールド座標
        const float branchRate = static_cast<float>(crackIndex) / static_cast<float>((std::max)(crackCount - kCrackRevealCountOffset, kBranchRateDenominatorMin)); // 枝先への進行度
        const float crackLength = settings_.crackLength + branchRate * settings_.crackLengthVariation; // 枝ごとの長さ
        const float crackWidth = settings_.crackWidth + static_cast<float>(crackIndex % kCrackWidthVariationCycle) * settings_.crackWidthVariation; // 枝ごとの太さ
        Vector4 crackColor = settings_.crackColor; // 亀裂の発光色
        crackColor.x = (std::min)(crackColor.x + branchRate * kCrackRedBoost, kEffectProgressMax);
        crackColor.z = (std::min)(crackColor.z + branchRate * kCrackBlueBoost, kEffectProgressMax);

        particleManager->EmitSpaceCrack(
            kHitParticleGroupName,
            crackPosition,
            kCrackAngles[crackIndex],
            crackLength,
            crackWidth,
            crackColor,
            settings_.crackLifeTime);
        ++cracksEmitted_;
    }
}

/// <summary>
/// 空間破砕時の多重リングと放射破片を発生する
/// </summary>
void TemporalRiftEffect::EmitRiftBurst()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 破砕表現を生成するパーティクル管理
    if (!particleManager) {
        return;
    }

    const uint32_t totalRingCount = static_cast<uint32_t>((std::max)(settings_.ringCount, kMinimumBurstEmitCount)); // 生成するリング総数
    const uint32_t innerRingCount = (totalRingCount + kInnerRingCountBias) / kRingColorGroupCount; // 青色の内側リング数
    const uint32_t outerRingCount = totalRingCount / kRingColorGroupCount; // 紫色の外側リング数
    particleManager->EmitRiftRing(
        kRingParticleGroupName,
        effectPosition_,
        innerRingCount,
        settings_.innerRingColor,
        kInnerRingStartRadius,
        kInnerRingEndRadius,
        kInnerRingLifeTime);
    particleManager->EmitRiftRing(
        kRingParticleGroupName,
        effectPosition_,
        outerRingCount,
        settings_.outerRingColor,
        kOuterRingStartRadius,
        kOuterRingEndRadius,
        kOuterRingLifeTime);
    particleManager->EmitRiftFragments(
        kHitParticleGroupName,
        effectPosition_,
        static_cast<uint32_t>((std::max)(settings_.fragmentCount, kMinimumBurstEmitCount)),
        settings_.fragmentColor,
        settings_.fragmentMinSpeed,
        settings_.fragmentMaxSpeed,
        settings_.fragmentLifeTime);
    if (!EffectParticleUtility::PlayGpuParticlePreset(kEffectOwnerName, kGpuSparkBurstPresetName, effectPosition_)
        && settings_.gpuFailureFallbackFragmentCount > 0) {
        particleManager->EmitRiftFragments(
            kHitParticleGroupName,
            effectPosition_,
            static_cast<uint32_t>(settings_.gpuFailureFallbackFragmentCount),
            settings_.fragmentColor,
            settings_.fragmentMinSpeed,
            settings_.fragmentMaxSpeed,
            settings_.fragmentLifeTime);
    }

}
