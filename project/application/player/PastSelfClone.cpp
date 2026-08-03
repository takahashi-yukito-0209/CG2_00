#include "PastSelfClone.h"

#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

#include <algorithm>
#include <cmath>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr float kMinimumPlaybackSpeed = 0.0f; // 最小再生速度
constexpr float kMaximumPlaybackSpeed = 4.0f; // 最大再生速度
constexpr float kImGuiPlaybackSpeedStep = 0.01f; // 再生速度調整の刻み
constexpr float kPlatformSnapTolerance = 0.08f; // 上面着地を許容する足元のずれ
constexpr float kPlatformHorizontalInset = 0.02f; // 端での不安定な着地を避ける内側余白
constexpr Vector3 kBaseBodyHalfSize = { 0.5f, 0.5f, 0.5f }; // 仮ブロックモデルの基準半サイズ

/// <summary>
/// Vector3を線形補間する。
/// </summary>
Vector3 LerpVector3(const Vector3& start, const Vector3& end, float rate)
{
    return {
        start.x + (end.x - start.x) * rate,
        start.y + (end.y - start.y) * rate,
        start.z + (end.z - start.z) * rate
    };
}

/// <summary>
/// 補間率を滑らかな変化へ変換する。
/// </summary>
float SmoothStep(float rate)
{
    const float clampedRate = (std::clamp)(rate, 0.0f, 1.0f); // 0から1に収めた補間率
    return clampedRate * clampedRate * (3.0f - 2.0f * clampedRate);
}

/// <summary>
/// 表示色を有効な範囲に収める。
/// </summary>
void ClampMaterialColor(Vector4* color, float minimumAlpha)
{
    if (!color) {
        return;
    }

    color->x = (std::clamp)(color->x, 0.0f, 1.0f);
    color->y = (std::clamp)(color->y, 0.0f, 1.0f);
    color->z = (std::clamp)(color->z, 0.0f, 1.0f);
    color->w = (std::clamp)(color->w, minimumAlpha, 1.0f);
}

/// <summary>
/// Transformのスケールから現在の半サイズを計算する。
/// </summary>
Vector3 CalculateBodyHalfSize(const Transform& transform)
{
    return {
        std::fabs(transform.scale.x) * kBaseBodyHalfSize.x,
        std::fabs(transform.scale.y) * kBaseBodyHalfSize.y,
        std::fabs(transform.scale.z) * kBaseBodyHalfSize.z
    };
}

/// <summary>
/// 分身と上面足場の横方向の奥行き範囲が重なっているか判定する。
/// </summary>
bool HasHorizontalOverlap(const Vector3& cloneCenter, const Vector3& cloneHalfSize, const StandablePlatform& platform)
{
    const float platformHalfX = (std::max)(platform.halfSize.x - kPlatformHorizontalInset, 0.0f); // 判定に使う足場X半幅
    const float platformHalfZ = (std::max)(platform.halfSize.z - kPlatformHorizontalInset, 0.0f); // 判定に使う足場Z半幅
    const bool overlapsX = std::fabs(cloneCenter.x - platform.center.x) <= cloneHalfSize.x + platformHalfX; // X方向の重なり
    const bool overlapsZ = std::fabs(cloneCenter.z - platform.center.z) <= cloneHalfSize.z + platformHalfZ; // Z方向の重なり
    return overlapsX && overlapsZ;
}
}

/// <summary>
/// デストラクタ
/// </summary>
PastSelfClone::~PastSelfClone() = default;

/// <summary>
/// 分身表示用の3Dオブジェクトを初期化する。
/// </summary>
void PastSelfClone::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const std::string& modelFileName)
{
    currentState_ = PlayerState {};
    object3d_ = std::make_unique<Object3d>(); // 分身表示用オブジェクト
    object3d_->Initialize(object3dCommon, imguiManager);
    object3d_->SetModel(modelFileName);
    object3d_->SetMaterialColor(materialColor_);
    object3d_->SetUseAlphaDiscard(false);
    ApplyStateMaterialColor();
}

/// <summary>
/// 分身が保持する表示用リソースを解放する。
/// </summary>
void PastSelfClone::Finalize()
{
    object3d_.reset();
    frames_.clear();
    currentState_ = PlayerState {};
    playbackTime_ = 0.0f;
    isPlaying_ = false;
    isVisible_ = false;
}

/// <summary>
/// 記録済みフレームの再生を開始する。
/// </summary>
bool PastSelfClone::Start(const std::vector<PastSelfFrame>& sourceFrames)
{
    if (sourceFrames.size() < 2) {
        return false;
    }

    frames_ = sourceFrames;
    const float firstTime = frames_.front().time; // 再生開始時刻を0秒へそろえるための基準
    for (PastSelfFrame& frame : frames_) {
        frame.time -= firstTime;
    }

    playbackTime_ = 0.0f;
    currentState_ = frames_.front().state;
    isPlaying_ = true;
    isVisible_ = true;
    ApplyStateMaterialColor();
    return true;
}

/// <summary>
/// 分身再生を停止する。
/// </summary>
void PastSelfClone::Stop()
{
    isPlaying_ = false;
    isVisible_ = false;
    playbackTime_ = 0.0f;
    ApplyStateMaterialColor();
}

/// <summary>
/// 再生状態に応じた分身表示色を反映する。
/// </summary>
void PastSelfClone::ApplyStateMaterialColor()
{
    if (!object3d_) {
        return;
    }

    const Math::Vector4 currentMaterialColor = (useStateMaterialColor_ && !isPlaying_) ? finishedMaterialColor_ : materialColor_; // 現在の再生状態に応じた表示色
    object3d_->SetMaterialColor(currentMaterialColor);
}

/// <summary>
/// 指定時刻のプレイヤー状態を取得する。
/// </summary>
PlayerState PastSelfClone::SampleState(float playbackTime) const
{
    if (frames_.empty()) {
        return PlayerState {};
    }
    if (playbackTime <= frames_.front().time) {
        return frames_.front().state;
    }
    if (playbackTime >= frames_.back().time) {
        return frames_.back().state;
    }

    for (size_t frameIndex = 0; frameIndex + 1 < frames_.size(); ++frameIndex) {
        const PastSelfFrame& currentFrame = frames_[frameIndex]; // 補間元の記録フレーム
        const PastSelfFrame& nextFrame = frames_[frameIndex + 1]; // 補間先の記録フレーム
        if (playbackTime < currentFrame.time || playbackTime > nextFrame.time) {
            continue;
        }

        const float frameDuration = nextFrame.time - currentFrame.time; // 2フレーム間の時間幅
        const float rate = frameDuration > 0.0f ? (playbackTime - currentFrame.time) / frameDuration : 0.0f; // 補間率
        const float sampleRate = smoothPlayback_ ? SmoothStep(rate) : (std::clamp)(rate, 0.0f, 1.0f); // 補間に使う最終的な補間率
        PlayerState state {}; // 補間後の状態
        state.transform.scale = LerpVector3(currentFrame.state.transform.scale, nextFrame.state.transform.scale, sampleRate);
        state.transform.rotate = LerpVector3(currentFrame.state.transform.rotate, nextFrame.state.transform.rotate, sampleRate);
        state.transform.translate = LerpVector3(currentFrame.state.transform.translate, nextFrame.state.transform.translate, sampleRate);
        state.isMoving = currentFrame.state.isMoving || nextFrame.state.isMoving;
        state.isGrounded = currentFrame.state.isGrounded || nextFrame.state.isGrounded;
        return state;
    }

    return frames_.back().state;
}

/// <summary>
/// 分身の再生状態を更新する。
/// </summary>
void PastSelfClone::Update(float deltaTime, const std::vector<StandablePlatform>& standablePlatforms)
{
    if (!isPlaying_ || frames_.size() < 2) {
        return;
    }

    const Vector3 cloneHalfSize = CalculateBodyHalfSize(currentState_.transform); // 更新前の分身半サイズ
    const float previousFootY = currentState_.transform.translate.y - cloneHalfSize.y; // 更新前の足元Y座標
    const float duration = frames_.back().time; // 記録全体の再生時間
    playbackTime_ += deltaTime * playbackSpeed_;
    if (playbackTime_ >= duration) {
        if (loopPlayback_ && duration > 0.0f) {
            playbackTime_ = 0.0f;
        } else {
            playbackTime_ = duration;
            isPlaying_ = false;
        }
    }

    currentState_ = SampleState(playbackTime_);
    ResolveLandingOnPlatforms(previousFootY, standablePlatforms);
    ApplyStateMaterialColor();
}

/// <summary>
/// 上面足場への着地だけを再生後の分身状態へ反映する。
/// </summary>
void PastSelfClone::ResolveLandingOnPlatforms(float previousFootY, const std::vector<StandablePlatform>& standablePlatforms)
{
    const Vector3 cloneHalfSize = CalculateBodyHalfSize(currentState_.transform); // 現在の分身半サイズ
    const float currentFootY = currentState_.transform.translate.y - cloneHalfSize.y; // 現在の足元Y座標
    bool hasLandingPlatform = false; // 着地候補があるか
    float landingTopY = currentFootY; // 採用する足場上面Y座標

    for (const StandablePlatform& platform : standablePlatforms) {
        if (!platform.enabled || !HasHorizontalOverlap(currentState_.transform.translate, cloneHalfSize, platform)) {
            continue;
        }

        const float platformTopY = platform.center.y + platform.halfSize.y; // 足場の上面Y座標
        const bool passedThroughTop = previousFootY >= platformTopY - kPlatformSnapTolerance && currentFootY <= platformTopY + kPlatformSnapTolerance; // 上面をまたいだか
        if (!passedThroughTop) {
            continue;
        }

        if (!hasLandingPlatform || platformTopY > landingTopY) {
            landingTopY = platformTopY;
            hasLandingPlatform = true;
        }
    }

    if (!hasLandingPlatform) {
        return;
    }

    currentState_.transform.translate.y = landingTopY + cloneHalfSize.y;
    currentState_.isGrounded = true;
}

/// <summary>
/// 分身を上面足場として扱うための情報を取得する。
/// </summary>
StandablePlatform PastSelfClone::GetStandablePlatform() const
{
    StandablePlatform platform {}; // 分身から作成する足場情報
    platform.center = currentState_.transform.translate;
    platform.halfSize = CalculateBodyHalfSize(currentState_.transform);
    platform.enabled = isVisible_ && standableEnabled_;
    return platform;
}

/// <summary>
/// 現在の分身状態を表示用Object3dへ反映する。
/// </summary>
void PastSelfClone::UpdateObject(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix)
{
    if (!object3d_ || !isVisible_) {
        return;
    }

    object3d_->SetScale(currentState_.transform.scale);
    object3d_->SetRotate(currentState_.transform.rotate);
    object3d_->SetTranslate(currentState_.transform.translate);
    object3d_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 分身を描画する。
/// </summary>
void PastSelfClone::Draw()
{
    if (!object3d_ || !isVisible_) {
        return;
    }

    Object3dCommon* object3dCommon = object3d_->GetObject3dCommon(); // 分身描画に使う共通描画状態
    if (!object3dCommon) {
        object3d_->Draw();
        return;
    }

    const BlendMode previousBlendMode = object3dCommon->GetBlendMode(); // 分身描画前のブレンドモード
    object3dCommon->SetBlendMode(BlendMode::Alpha);
    object3d_->Draw();
    object3dCommon->SetBlendMode(previousBlendMode);
}

/// <summary>
/// ImGuiで分身状態を表示する。
/// </summary>
void PastSelfClone::DrawImGui()
{
#ifdef USE_IMGUI
    const char* playbackStateLabel = !isVisible_ ? "Hidden" : (isPlaying_ ? "Playing" : "Finished"); // ImGuiに表示する分身の再生状態
    ImGui::Text("State: %s", playbackStateLabel);
    ImGui::Text("Playing: %s", isPlaying_ ? "true" : "false");
    ImGui::Text("Visible: %s", isVisible_ ? "true" : "false");
    ImGui::Text("Frames: %zu", frames_.size());
    ImGui::Text("Playback Time: %.2f sec", playbackTime_);
    ImGui::Text("Duration: %.2f sec", frames_.empty() ? 0.0f : frames_.back().time);
    ImGui::Text("Standable: %s", (isVisible_ && standableEnabled_) ? "true" : "false");
    ImGui::DragFloat("Playback Speed", &playbackSpeed_, kImGuiPlaybackSpeedStep, kMinimumPlaybackSpeed, kMaximumPlaybackSpeed);
    ImGui::Checkbox("Smooth Playback", &smoothPlayback_);
    ImGui::Checkbox("Loop Playback", &loopPlayback_);
    ImGui::Checkbox("Standable Enabled", &standableEnabled_);
    ImGui::Checkbox("State Color", &useStateMaterialColor_);
    ImGui::ColorEdit4("Playing Color", &materialColor_.x);
    ImGui::ColorEdit4("Finished Color", &finishedMaterialColor_.x);
    playbackSpeed_ = (std::clamp)(playbackSpeed_, kMinimumPlaybackSpeed, kMaximumPlaybackSpeed);
    ClampMaterialColor(&materialColor_, 0.1f);
    ClampMaterialColor(&finishedMaterialColor_, 0.05f);
    ApplyStateMaterialColor();
#endif
}