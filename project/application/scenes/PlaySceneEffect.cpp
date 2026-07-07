#include "PlayScene.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../../engine/3d/Camera.h"
#include "../../engine/utility/mathUtility.h"

#include <algorithm>

using namespace MyEngine;
using namespace Math;
namespace {
constexpr Vector2 kScreenCenterUv = { 0.5f, 0.5f }; // 変換できない場合に使用する画面中央
constexpr float kMinimumClipW = 0.0001f; // カメラ背面とゼロ除算を判定する最小値
constexpr float kNdcToUvScale = 0.5f; // NDC座標をUV座標へ変換する倍率
constexpr float kNdcToUvOffset = 0.5f; // NDC座標をUV座標へ変換するオフセット

/// <summary>
/// ワールド座標を画面UV座標へ変換する
/// </summary>
Vector2 CalculateScreenUvFromWorldPosition(const Camera* camera, const Vector3& worldPosition)
{
    if (!camera) {
        return kScreenCenterUv;
    }

    const Matrix4x4 viewProjection = MathUtil::Multiply(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()); // ワールド座標をクリップ座標へ変換する行列
    const float clipX = worldPosition.x * viewProjection.m[0][0]
        + worldPosition.y * viewProjection.m[1][0]
        + worldPosition.z * viewProjection.m[2][0]
        + viewProjection.m[3][0]; // クリップ座標のX成分
    const float clipY = worldPosition.x * viewProjection.m[0][1]
        + worldPosition.y * viewProjection.m[1][1]
        + worldPosition.z * viewProjection.m[2][1]
        + viewProjection.m[3][1]; // クリップ座標のY成分
    const float clipW = worldPosition.x * viewProjection.m[0][3]
        + worldPosition.y * viewProjection.m[1][3]
        + worldPosition.z * viewProjection.m[2][3]
        + viewProjection.m[3][3]; // 透視除算に使用するW成分
    if (clipW <= kMinimumClipW) {
        return kScreenCenterUv;
    }

    Vector2 screenUv = {
        (clipX / clipW) * kNdcToUvScale + kNdcToUvOffset,
        -(clipY / clipW) * kNdcToUvScale + kNdcToUvOffset,
    }; // NDC座標を画面UV座標へ変換した結果
    screenUv.x = (std::clamp)(screenUv.x, 0.0f, 1.0f);
    screenUv.y = (std::clamp)(screenUv.y, 0.0f, 1.0f);
    return screenUv;
}
}

/// <summary>
/// エフェクト操作用のImGuiを描画する
/// </summary>
void PlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Effect Controller");

    int selectedEffectIndex = static_cast<int>(selectedEffectType_); // ImGuiで編集中のエフェクト番号
    const char* effectNames[] = {
        "Dimensional Shatter",
        "Time Reversal",
        "Time Stop",
    }; // 選択可能なエフェクト名
    if (!IsAnyEffectPlaying()
        && ImGui::Combo(
            "Effect Type",
            &selectedEffectIndex,
            effectNames,
            IM_ARRAYSIZE(effectNames))) {
        selectedEffectType_ = static_cast<EffectType>(selectedEffectIndex);
    }

    ImGui::Text("Trigger Key: R");
    if (!IsAnyEffectPlaying()) {
        if (ImGui::Button("Play Effect")) {
            StartSelectedEffect();
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("Play Effect");
        ImGui::EndDisabled();
    }

    if (selectedEffectType_ == EffectType::DimensionalShatter) {
        temporalRiftEffect_.DrawImGui();
        ImGui::End();
        return;
    }
    if (selectedEffectType_ == EffectType::TimeStop) {
        timeStopEffect_.DrawImGui();
        ImGui::End();
        return;
    }
    if (selectedEffectType_ == EffectType::TimeReversal) {
        timeReversalEffect_.DrawImGui();
        ImGui::End();
        return;
    }

    ImGui::End();
#endif
}

/// <summary>
/// ImGuiで選択中のエフェクトを開始する
/// </summary>
void PlayScene::StartSelectedEffect()
{
    switch (selectedEffectType_) {
    case EffectType::DimensionalShatter:
        StartTemporalRiftEffect();
        break;
    case EffectType::TimeReversal:
        StartTimeReversalEffect();
        break;
    case EffectType::TimeStop:
        StartTimeStopEffect();
        break;
    }
}

/// <summary>
/// いずれかのエフェクトが再生中か確認する
/// </summary>
bool PlayScene::IsAnyEffectPlaying() const
{
    return temporalRiftEffect_.IsPlaying()
        || timeReversalEffect_.IsPlaying()
        || timeStopEffect_.IsPlaying();
}

/// <summary>
/// 時間停止エフェクトを開始する
/// </summary>
void PlayScene::StartTimeStopEffect()
{
    timeStopEffect_.Start(postProcess_, CalculateWorldScreenUv(timeStopEffect_.GetEffectPosition()));
}

/// <summary>
/// 時間停止エフェクトの状態を更新する
/// </summary>
void PlayScene::UpdateTimeStopEffect(float deltaTime)
{
    timeStopEffect_.Update(deltaTime, postProcess_);
}

/// <summary>
/// 時間停止中か確認する
/// </summary>
bool PlayScene::IsTimeStopped() const
{
    return timeStopEffect_.IsStopped();
}

/// <summary>
/// 時間逆行エフェクトを開始する
/// </summary>
void PlayScene::StartTimeReversalEffect()
{
    timeReversalEffect_.Start(postProcess_, timeReversalSprites_.size());
}

/// <summary>
/// 時間逆行エフェクトの状態を更新する
/// </summary>
void PlayScene::UpdateTimeReversalEffect(float deltaTime)
{
    timeReversalEffect_.Update(deltaTime, postProcess_, objects3d_);
}

/// <summary>
/// 時間逆行用スプライトを更新する
/// </summary>
void PlayScene::UpdateTimeReversalSprites()
{
    timeReversalEffect_.UpdateSprites(
        timeReversalSprites_,
        timeReversalAfterimageSprites_,
        timeReversalConvergenceSprite_.get(),
        [this](const Vector3& worldPosition) {
            return CalculateWorldScreenUv(worldPosition);
        });
}

/// <summary>
/// 時間逆行対象のTransform履歴を更新する
/// </summary>
void PlayScene::UpdateTimeReversalTransformHistory()
{
    timeReversalEffect_.UpdateTransformHistory(
        objects3d_,
        !temporalRiftEffect_.IsPlaying());
}

/// <summary>
/// 時間逆行用スプライトを描画する
/// </summary>
void PlayScene::DrawTimeReversalParticles()
{
    timeReversalEffect_.DrawParticles(
        ctx_.spriteCommon,
        timeReversalSprites_,
        timeReversalAfterimageSprites_,
        timeReversalConvergenceSprite_.get());
}

/// <summary>
/// 時空破砕エフェクトを開始する
/// </summary>
void PlayScene::StartTemporalRiftEffect()
{
    temporalRiftEffect_.SetScreenUv(CalculateTemporalRiftScreenUv());
    temporalRiftEffect_.Start(postProcess_, objects3d_, temporalRiftEffect_.GetScreenUv());
}

/// <summary>
/// 時空破砕エフェクトの状態を更新する
/// </summary>
void PlayScene::UpdateTemporalRiftEffect(float deltaTime)
{
    temporalRiftEffect_.Update(deltaTime, postProcess_, ctx_.camera);
}

/// <summary>
/// 時空破砕の発生位置を画面UV座標へ変換する
/// </summary>
Math::Vector2 PlayScene::CalculateTemporalRiftScreenUv() const
{
    return CalculateScreenUvFromWorldPosition(ctx_.camera, temporalRiftEffect_.GetEffectPosition());
}

/// <summary>
/// ワールド座標を画面UV座標へ変換する
/// </summary>
Math::Vector2 PlayScene::CalculateWorldScreenUv(const Math::Vector3& worldPosition) const
{
    return CalculateScreenUvFromWorldPosition(ctx_.camera, worldPosition);
}

/// <summary>
/// 時間ずれ対象のTransform履歴を更新する
/// </summary>
void PlayScene::UpdateTemporalAfterimages()
{
    temporalRiftEffect_.UpdateAfterimages(objects3d_);
}

/// <summary>
/// Transform履歴から残像スプライトを更新する
/// </summary>
void PlayScene::UpdateAfterimageSprites()
{
    temporalRiftEffect_.UpdateAfterimageSprites(
        temporalAfterimageSprites_,
        [this](const Vector3& worldPosition) {
            return CalculateWorldScreenUv(worldPosition);
        });
}

/// <summary>
/// Transform履歴による残像を描画する
/// </summary>
void PlayScene::DrawTemporalAfterimages()
{
    temporalRiftEffect_.DrawAfterimages(ctx_.spriteCommon, temporalAfterimageSprites_);
}

/// <summary>
/// ヒットストップとカメラシェイクを更新する
/// </summary>
void PlayScene::UpdateImpactResponse(float deltaTime)
{
    temporalRiftEffect_.UpdateImpact(deltaTime, ctx_.camera);
}

/// <summary>
/// カメラシェイクを終了してカメラ位置を復元する
/// </summary>
void PlayScene::StopCameraShake()
{
    temporalRiftEffect_.StopCameraShake(ctx_.camera);
}