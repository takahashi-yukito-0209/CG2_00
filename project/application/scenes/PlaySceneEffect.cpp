#include "PlayScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/utility/mathUtility.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr Vector2 kScreenCenterUv = { 0.5f, 0.5f }; // 変換できない場合に使用する画面中央
constexpr float kMinimumClipW = 0.0001f; // カメラ背面とゼロ除算を判定する最小値
constexpr float kNdcToUvScale = 0.5f; // NDC座標をUV座標へ変換する倍率
constexpr float kNdcToUvOffset = 0.5f; // NDC座標をUV座標へ変換するオフセット
constexpr float kScreenUvMin = 0.0f; // 画面UVの最小値
constexpr float kScreenUvMax = 1.0f; // 画面UVの最大値
constexpr Vector2 kCenteredSpriteAnchor = { 0.5f, 0.5f }; // 中心基準で表示するスプライトのアンカー
constexpr Vector2 kTemporalSpriteDefaultSize = { 1.0f, 1.0f }; // 時間演出スプライトの初期サイズ
constexpr Vector4 kHiddenSpriteColor = { 0.0f, 0.0f, 0.0f, 0.0f }; // 非表示状態にするスプライト色
constexpr int kMaximumTemporalAfterimageCount = 8; // 時空破砕で保持できる最大残像数
constexpr int kMaximumTimeReversalParticleCount = 128; // 時間逆流で保持できる最大粒子数
constexpr int kMaximumTimeReversalAfterimageCount = 3; // 1粒子ごとに保持する最大残像数
constexpr const char* kCircleFlashTextureName = "circle2.png"; // 発光系スプライトに使用するテクスチャ名


/// <summary>
/// 非表示状態の時間演出スプライトを作成する
/// </summary>
std::unique_ptr<Sprite> CreateHiddenTemporalSprite(const SceneContext& ctx, const std::string& textureName)
{
    auto sprite = std::make_unique<Sprite>(); // 作成する時間演出用スプライト
    sprite->Initialize(ctx.spriteCommon, textureName, ctx.imguiManager);
    sprite->SetAnchorPoint(kCenteredSpriteAnchor);
    sprite->SetSize(kTemporalSpriteDefaultSize);
    sprite->SetColor(kHiddenSpriteColor);
    sprite->Update();
    return sprite;
}

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
    screenUv.x = (std::clamp)(screenUv.x, kScreenUvMin, kScreenUvMax);
    screenUv.y = (std::clamp)(screenUv.y, kScreenUvMin, kScreenUvMax);
    return screenUv;
}

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
    EnsureTimeReversalSprites();
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
    if (!timeReversalEffect_.IsPlaying()) {
        return;
    }

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
    if (!timeReversalEffect_.IsPlaying()) {
        return;
    }

    timeReversalEffect_.DrawParticles(
        ctx_.spriteCommon,
        timeReversalSprites_,
        timeReversalAfterimageSprites_,
        timeReversalConvergenceSprite_.get());
}

/// <summary>
/// 時間演出用スプライトを初期化する
/// </summary>
void PlayScene::InitializeTemporalEffectSprites()
{
    temporalAfterimageSprites_.reserve(kMaximumTemporalAfterimageCount);
    timeReversalSprites_.reserve(kMaximumTimeReversalParticleCount);
    timeReversalAfterimageSprites_.reserve(kMaximumTimeReversalParticleCount * kMaximumTimeReversalAfterimageCount);
}

/// <summary>
/// 時空破砕で使用する残像スプライトを必要になった時点で作成する。
/// </summary>
void PlayScene::EnsureTemporalRiftSprites()
{
    while (temporalAfterimageSprites_.size() < kMaximumTemporalAfterimageCount) {
        auto afterimageSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時空破砕の残像表示に使うスプライト
        temporalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }
}

/// <summary>
/// 時間逆行で使用するスプライトを必要になった時点で作成する。
/// </summary>
void PlayScene::EnsureTimeReversalSprites()
{
    while (timeReversalSprites_.size() < kMaximumTimeReversalParticleCount) {
        auto particleSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時間逆行の粒子表示に使うスプライト
        timeReversalSprites_.push_back(std::move(particleSprite));
    }

    const size_t maximumRewindAfterimageSpriteCount = static_cast<size_t>(kMaximumTimeReversalParticleCount * kMaximumTimeReversalAfterimageCount); // 時間逆行の軌跡表示に必要な残像数
    while (timeReversalAfterimageSprites_.size() < maximumRewindAfterimageSpriteCount) {
        auto afterimageSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時間逆行の軌跡表示に使うスプライト
        timeReversalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }

    if (!timeReversalConvergenceSprite_) {
        timeReversalConvergenceSprite_ = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName);
    }
}
/// <summary>
/// 時空破砕エフェクトを開始する
/// </summary>
void PlayScene::StartTemporalRiftEffect()
{
    EnsureTemporalRiftSprites();
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
    if (!temporalRiftEffect_.IsPlaying()) {
        return;
    }

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
    if (!temporalRiftEffect_.IsPlaying()) {
        return;
    }

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
