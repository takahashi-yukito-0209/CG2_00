#include "StageGimmicks.h"

#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>

using namespace MyEngine;

namespace {
/// <summary>
/// 指定位置が箱形範囲内にあるか判定する。
/// </summary>
bool IsPointInsideBox(const Math::Vector3& point, const Math::Vector3& center, const Math::Vector3& halfSize)
{
    return std::fabs(point.x - center.x) <= halfSize.x &&
        std::fabs(point.y - center.y) <= halfSize.y &&
        std::fabs(point.z - center.z) <= halfSize.z;
}

/// <summary>
/// スケールから箱形の半サイズを計算する。
/// </summary>
Math::Vector3 CalculateBoxHalfSize(const Math::Vector3& scale)
{
    return {
        std::fabs(scale.x) * 0.5f,
        std::fabs(scale.y) * 0.5f,
        std::fabs(scale.z) * 0.5f
    };
}

/// <summary>
/// 現在座標を目標座標へ最大移動量の範囲で近づける。
/// </summary>
Math::Vector3 MoveTowards(const Math::Vector3& current, const Math::Vector3& target, float maxDistance)
{
    const float deltaX = target.x - current.x; // 目標までのX方向距離
    const float deltaY = target.y - current.y; // 目標までのY方向距離
    const float deltaZ = target.z - current.z; // 目標までのZ方向距離
    const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ); // 目標までの直線距離
    if (distance <= maxDistance || distance <= 0.0001f) {
        return target;
    }

    const float moveRatio = maxDistance / distance; // 最大移動量を距離へ換算した比率
    return {
        current.x + deltaX * moveRatio,
        current.y + deltaY * moveRatio,
        current.z + deltaZ * moveRatio
    };
}

/// <summary>
/// 仮ギミック用のブロックオブジェクトを作成する。
/// </summary>
std::unique_ptr<Object3d> CreateGimmickObject(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, uint32_t objectId, const std::string& modelFileName, const Math::Vector3& scale, const Math::Vector3& translate, const Math::Vector4& color)
{
    std::unique_ptr<Object3d> object = std::make_unique<Object3d>(); // 生成するギミック表示用オブジェクト
    object->SetObjectId(objectId);
    object->Initialize(object3dCommon, imguiManager);
    object->SetModel(modelFileName);
    object->SetScale(scale);
    object->SetRotate({ 0.0f, 0.0f, 0.0f });
    object->SetTranslate(translate);
    object->SetMaterialColor(color);
    object->SetUseTexture(false);
    object->SetEnableLighting(false);
    object->SetUseAlphaDiscard(false);
    return object;
}

/// <summary>
/// 指定した3DオブジェクトをAlphaブレンドで描画する。
/// </summary>
void DrawObjectWithAlphaBlend(Object3d* object)
{
    if (!object) {
        return;
    }

    Object3dCommon* object3dCommon = object->GetObject3dCommon(); // 描画に使う共通描画状態
    if (!object3dCommon) {
        object->Draw();
        return;
    }

    const BlendMode previousBlendMode = object3dCommon->GetBlendMode(); // 描画前のブレンドモード
    object3dCommon->SetBlendMode(BlendMode::Alpha);
    object->Draw();
    object3dCommon->SetBlendMode(previousBlendMode);
}

/// <summary>
/// 編集された表示Transformを箱形入力判定へ同期する。
/// </summary>
void ApplyEditedSwitchTransform(Object3d* object, Math::Vector3& previousScale, Math::Vector3& previousTranslate, Math::Vector3& volumeCenter, Math::Vector3& volumeHalfSize)
{
    if (!object) {
        return;
    }

    const Math::Vector3 editedScale = object->GetScale(); // 編集後の表示スケール
    const Math::Vector3 editedTranslate = object->GetTranslate(); // 編集後の表示座標
    const Math::Vector3 translateDelta = editedTranslate - previousTranslate; // 判定中心へ加える移動量
    constexpr float kMinimumScale = 0.0001f; // 拡縮率計算で除算可能とみなす最小値
    const Math::Vector3 scaleRatio = { // 判定範囲へ反映する各軸の拡縮率
        std::fabs(previousScale.x) > kMinimumScale ? std::fabs(editedScale.x / previousScale.x) : 1.0f,
        std::fabs(previousScale.y) > kMinimumScale ? std::fabs(editedScale.y / previousScale.y) : 1.0f,
        std::fabs(previousScale.z) > kMinimumScale ? std::fabs(editedScale.z / previousScale.z) : 1.0f,
    };
    volumeCenter += translateDelta;
    volumeHalfSize = {
        volumeHalfSize.x * scaleRatio.x,
        volumeHalfSize.y * scaleRatio.y,
        volumeHalfSize.z * scaleRatio.z,
    };
    previousScale = editedScale;
    previousTranslate = editedTranslate;
}
}

/// <summary>
/// スイッチの表示、判定範囲、色を初期化する。
/// </summary>
void BoxSwitchGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const BoxSwitchGimmickDesc& desc)
{
    volumeCenter_ = desc.volumeCenter;
    volumeHalfSize_ = desc.volumeHalfSize;
    inactiveColor_ = desc.inactiveColor;
    activeColor_ = desc.activeColor;
    playerOnlyColor_ = desc.playerOnlyColor;
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, desc.scale, desc.translate, inactiveColor_);
    editorScale_ = desc.scale;
    editorTranslate_ = desc.translate;
    Reset();
}

/// <summary>
/// スイッチが保持する表示用リソースを解放する。
/// </summary>
void BoxSwitchGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
/// </summary>
void BoxSwitchGimmick::ApplyEditorTransform()
{
    ApplyEditedSwitchTransform(object_.get(), editorScale_, editorTranslate_, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// プレイヤーと可視分身の位置からスイッチ状態を更新する。
/// </summary>
void BoxSwitchGimmick::Update(const PlayerState& playerState, std::span<const PlayerState> cloneStates)
{
    playerOnSwitch_ = Contains(playerState);
    cloneOnSwitch_ = std::any_of(cloneStates.begin(), cloneStates.end(), [this](const PlayerState& cloneState) {
        return Contains(cloneState);
    });
    active_ = cloneOnSwitch_;
    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void BoxSwitchGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// スイッチを描画する。
/// </summary>
void BoxSwitchGimmick::Draw()
{
    if (!object_) {
        return;
    }

    object_->Draw();
}

/// <summary>
/// スイッチ状態を初期状態へ戻す。
/// </summary>
void BoxSwitchGimmick::Reset()
{
    active_ = false;
    playerOnSwitch_ = false;
    cloneOnSwitch_ = false;
    ApplyVisual();
}

/// <summary>
/// 指定したプレイヤー状態がスイッチ判定内にあるか判定する。
/// </summary>
bool BoxSwitchGimmick::Contains(const PlayerState& state) const
{
    return IsPointInsideBox(state.transform.translate, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// 現在状態に応じた表示色を反映する。
/// </summary>
void BoxSwitchGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    Math::Vector4 color = inactiveColor_; // 現在状態から選択した表示色
    if (cloneOnSwitch_) {
        color = activeColor_;
    } else if (playerOnSwitch_) {
        color = playerOnlyColor_;
    }
    object_->SetMaterialColor(color);
}

/// <summary>
/// 時間差スイッチの表示、判定範囲、色、維持時間を初期化する。
/// </summary>
void TimedSwitchGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const TimedSwitchGimmickDesc& desc)
{
    volumeCenter_ = desc.volumeCenter;
    volumeHalfSize_ = desc.volumeHalfSize;
    inactiveColor_ = desc.inactiveColor;
    activeColor_ = desc.activeColor;
    triggerColor_ = desc.triggerColor;
    holdSeconds_ = (std::max)(desc.holdSeconds, 0.0f);
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, desc.scale, desc.translate, inactiveColor_);
    editorScale_ = desc.scale;
    editorTranslate_ = desc.translate;
    Reset();
}

/// <summary>
/// 時間差スイッチが保持する表示用リソースを解放する。
/// </summary>
void TimedSwitchGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
/// </summary>
void TimedSwitchGimmick::ApplyEditorTransform()
{
    ApplyEditedSwitchTransform(object_.get(), editorScale_, editorTranslate_, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// 可視分身の位置と経過時間からスイッチ状態を更新する。
/// </summary>
void TimedSwitchGimmick::Update(float deltaTime, std::span<const PlayerState> cloneStates)
{
    cloneOnSwitch_ = std::any_of(cloneStates.begin(), cloneStates.end(), [this](const PlayerState& cloneState) {
        return Contains(cloneState);
    });
    if (cloneOnSwitch_) {
        remainingSeconds_ = holdSeconds_;
    } else {
        remainingSeconds_ = (std::max)(remainingSeconds_ - deltaTime, 0.0f);
    }

    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void TimedSwitchGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 時間差スイッチを描画する。
/// </summary>
void TimedSwitchGimmick::Draw()
{
    if (!object_) {
        return;
    }

    object_->Draw();
}

/// <summary>
/// 時間差スイッチ状態を初期状態へ戻す。
/// </summary>
void TimedSwitchGimmick::Reset()
{
    remainingSeconds_ = 0.0f;
    cloneOnSwitch_ = false;
    ApplyVisual();
}

/// <summary>
/// 指定したプレイヤー状態がスイッチ判定内にあるか判定する。
/// </summary>
bool TimedSwitchGimmick::Contains(const PlayerState& state) const
{
    return IsPointInsideBox(state.transform.translate, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// 現在状態に応じた表示色を反映する。
/// </summary>
void TimedSwitchGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    Math::Vector4 color = inactiveColor_; // 現在状態から選択した表示色
    if (cloneOnSwitch_) {
        color = triggerColor_;
    } else if (remainingSeconds_ > 0.0f) {
        color = activeColor_;
    }
    object_->SetMaterialColor(color);
}

/// <summary>
/// トグルスイッチの表示、判定範囲、色を初期化する。
/// </summary>
void ToggleSwitchGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const ToggleSwitchGimmickDesc& desc)
{
    volumeCenter_ = desc.volumeCenter;
    volumeHalfSize_ = desc.volumeHalfSize;
    inactiveColor_ = desc.inactiveColor;
    activeColor_ = desc.activeColor;
    pressedColor_ = desc.pressedColor;
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, desc.scale, desc.translate, inactiveColor_);
    editorScale_ = desc.scale;
    editorTranslate_ = desc.translate;
    Reset();
}

/// <summary>
/// トグルスイッチが保持する表示用リソースを解放する。
/// </summary>
void ToggleSwitchGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
/// </summary>
void ToggleSwitchGimmick::ApplyEditorTransform()
{
    ApplyEditedSwitchTransform(object_.get(), editorScale_, editorTranslate_, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// 可視分身の位置からスイッチ状態を更新する。
/// </summary>
void ToggleSwitchGimmick::Update(std::span<const PlayerState> cloneStates)
{
    cloneOnSwitch_ = std::any_of(cloneStates.begin(), cloneStates.end(), [this](const PlayerState& cloneState) {
        return Contains(cloneState);
    });
    const bool cloneEnteredSwitch = cloneOnSwitch_ && !cloneOnSwitchLastFrame_; // 分身が判定外から入った瞬間か
    if (cloneEnteredSwitch) {
        active_ = !active_;
    }
    cloneOnSwitchLastFrame_ = cloneOnSwitch_;
    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void ToggleSwitchGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// トグルスイッチを描画する。
/// </summary>
void ToggleSwitchGimmick::Draw()
{
    if (!object_) {
        return;
    }

    object_->Draw();
}

/// <summary>
/// トグルスイッチをOFF状態へ戻す。
/// </summary>
void ToggleSwitchGimmick::Reset()
{
    active_ = false;
    cloneOnSwitch_ = false;
    cloneOnSwitchLastFrame_ = false;
    ApplyVisual();
}

/// <summary>
/// 指定したプレイヤー状態がスイッチ判定内にあるか判定する。
/// </summary>
bool ToggleSwitchGimmick::Contains(const PlayerState& state) const
{
    return IsPointInsideBox(state.transform.translate, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// 現在状態に応じた表示色を反映する。
/// </summary>
void ToggleSwitchGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    Math::Vector4 color = active_ ? activeColor_ : inactiveColor_; // ON/OFF状態から選択した表示色
    if (cloneOnSwitch_) {
        color = pressedColor_;
    }
    object_->SetMaterialColor(color);
}

/// <summary>
/// 重さスイッチの表示、判定範囲、色を初期化する。
/// </summary>
void WeightSwitchGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const WeightSwitchGimmickDesc& desc)
{
    volumeCenter_ = desc.volumeCenter;
    volumeHalfSize_ = desc.volumeHalfSize;
    inactiveColor_ = desc.inactiveColor;
    partialColor_ = desc.partialColor;
    activeColor_ = desc.activeColor;
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, desc.scale, desc.translate, inactiveColor_);
    editorScale_ = desc.scale;
    editorTranslate_ = desc.translate;
    Reset();
}

/// <summary>
/// 重さスイッチが保持する表示用リソースを解放する。
/// </summary>
void WeightSwitchGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
/// </summary>
void WeightSwitchGimmick::ApplyEditorTransform()
{
    ApplyEditedSwitchTransform(object_.get(), editorScale_, editorTranslate_, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// プレイヤーと可視分身の位置からスイッチ状態を更新する。
/// </summary>
void WeightSwitchGimmick::Update(const PlayerState& playerState, std::span<const PlayerState> cloneStates)
{
    playerOnSwitch_ = Contains(playerState);
    cloneOnSwitch_ = std::any_of(cloneStates.begin(), cloneStates.end(), [this](const PlayerState& cloneState) {
        return Contains(cloneState);
    });
    active_ = playerOnSwitch_ && cloneOnSwitch_;
    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void WeightSwitchGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 重さスイッチを描画する。
/// </summary>
void WeightSwitchGimmick::Draw()
{
    if (!object_) {
        return;
    }

    object_->Draw();
}

/// <summary>
/// 重さスイッチ状態を初期状態へ戻す。
/// </summary>
void WeightSwitchGimmick::Reset()
{
    active_ = false;
    playerOnSwitch_ = false;
    cloneOnSwitch_ = false;
    ApplyVisual();
}

/// <summary>
/// 指定したプレイヤー状態がスイッチ判定内にあるか判定する。
/// </summary>
bool WeightSwitchGimmick::Contains(const PlayerState& state) const
{
    return IsPointInsideBox(state.transform.translate, volumeCenter_, volumeHalfSize_);
}

/// <summary>
/// 現在状態に応じた表示色を反映する。
/// </summary>
void WeightSwitchGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    Math::Vector4 color = inactiveColor_; // 現在状態から選択した表示色
    if (active_) {
        color = activeColor_;
    } else if (playerOnSwitch_ || cloneOnSwitch_) {
        color = partialColor_;
    }
    object_->SetMaterialColor(color);
}

/// <summary>
/// 扉の表示、衝突範囲、色を初期化する。
/// </summary>
void LinkedDoorGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const LinkedDoorGimmickDesc& desc)
{
    scale_ = desc.scale;
    translate_ = desc.translate;
    closedColor_ = desc.closedColor;
    openColor_ = desc.openColor;
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, scale_, translate_, closedColor_);
    Reset();
}

/// <summary>
/// 扉が保持する表示用リソースを解放する。
/// </summary>
void LinkedDoorGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果を扉の衝突判定へ反映する。
/// </summary>
void LinkedDoorGimmick::ApplyEditorTransform()
{
    if (!object_) {
        return;
    }
    scale_ = object_->GetScale();
    translate_ = object_->GetTranslate();
}

/// <summary>
/// 入力状態に合わせて扉の開閉状態を更新する。
/// </summary>
void LinkedDoorGimmick::Update(bool shouldOpen)
{
    open_ = shouldOpen;
    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void LinkedDoorGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 扉を描画する。
/// </summary>
void LinkedDoorGimmick::Draw()
{
    if (!object_) {
        return;
    }

    if (!open_) {
        object_->Draw();
        return;
    }

    DrawWithAlphaBlend();
}

/// <summary>
/// 扉状態を閉じた状態へ戻す。
/// </summary>
void LinkedDoorGimmick::Reset()
{
    open_ = false;
    ApplyVisual();
}

/// <summary>
/// 閉じている扉の全面コライダーを取得する。
/// </summary>
SolidCollider LinkedDoorGimmick::GetSolidCollider() const
{
    SolidCollider collider {}; // 扉から作成する全面コライダー
    collider.center = translate_;
    collider.halfSize = CalculateBoxHalfSize(scale_);
    collider.enabled = !open_;
    return collider;
}

/// <summary>
/// 現在状態に応じた表示色を反映する。
/// </summary>
void LinkedDoorGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    const Math::Vector4 color = open_ ? openColor_ : closedColor_; // 開閉状態に応じた表示色
    object_->SetMaterialColor(color);
}

/// <summary>
/// 指定した3DオブジェクトをAlphaブレンドで描画する。
/// </summary>
void LinkedDoorGimmick::DrawWithAlphaBlend()
{
    DrawObjectWithAlphaBlend(object_.get());
}

/// <summary>
/// 橋の表示、衝突範囲、色を初期化する。
/// </summary>
void LinkedBridgeGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const LinkedBridgeGimmickDesc& desc)
{
    scale_ = desc.scale;
    translate_ = desc.translate;
    retractedColor_ = desc.retractedColor;
    deployedColor_ = desc.deployedColor;
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, scale_, translate_, retractedColor_);
    Reset();
}

/// <summary>
/// 橋が保持する表示用リソースを解放する。
/// </summary>
void LinkedBridgeGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果を橋の衝突判定へ反映する。
/// </summary>
void LinkedBridgeGimmick::ApplyEditorTransform()
{
    if (!object_) {
        return;
    }
    scale_ = object_->GetScale();
    translate_ = object_->GetTranslate();
}

/// <summary>
/// 入力状態に合わせて橋の展開状態を更新する。
/// </summary>
void LinkedBridgeGimmick::Update(bool shouldDeploy)
{
    deployed_ = shouldDeploy;
    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void LinkedBridgeGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 橋を描画する。
/// </summary>
void LinkedBridgeGimmick::Draw()
{
    if (!object_) {
        return;
    }

    if (deployed_) {
        object_->Draw();
        return;
    }

    DrawWithAlphaBlend();
}

/// <summary>
/// 橋を未展開状態へ戻す。
/// </summary>
void LinkedBridgeGimmick::Reset()
{
    deployed_ = false;
    ApplyVisual();
}

/// <summary>
/// 展開中の橋の全面コライダーを取得する。
/// </summary>
SolidCollider LinkedBridgeGimmick::GetSolidCollider() const
{
    SolidCollider collider {}; // 橋から作成する全面コライダー
    collider.center = translate_;
    collider.halfSize = CalculateBoxHalfSize(scale_);
    collider.enabled = deployed_;
    return collider;
}

/// <summary>
/// 現在状態に応じた表示色を反映する。
/// </summary>
void LinkedBridgeGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    const Math::Vector4 color = deployed_ ? deployedColor_ : retractedColor_; // 展開状態に応じた表示色
    object_->SetMaterialColor(color);
}

/// <summary>
/// 指定した3DオブジェクトをAlphaブレンドで描画する。
/// </summary>
void LinkedBridgeGimmick::DrawWithAlphaBlend()
{
    DrawObjectWithAlphaBlend(object_.get());
}

/// <summary>
/// 昇降足場の表示、移動範囲、速度を初期化する。
/// </summary>
void MovingPlatformGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const MovingPlatformGimmickDesc& desc)
{
    scale_ = desc.scale;
    lowerTranslate_ = desc.lowerTranslate;
    upperTranslate_ = desc.upperTranslate;
    inactiveColor_ = desc.inactiveColor;
    activeColor_ = desc.activeColor;
    moveSpeed_ = (std::max)(desc.moveSpeed, 0.0f);
    upperWaitSeconds_ = (std::max)(desc.upperWaitSeconds, 0.0f);
    lowerWaitSeconds_ = (std::max)(desc.lowerWaitSeconds, 0.0f);
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, scale_, lowerTranslate_, inactiveColor_);
    Reset();
}

/// <summary>
/// 昇降足場が保持する表示用リソースを解放する。
/// </summary>
void MovingPlatformGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果を昇降範囲と衝突判定へ反映する。
/// </summary>
void MovingPlatformGimmick::ApplyEditorTransform()
{
    if (!object_) {
        return;
    }

    const Math::Vector3 editedTranslate = object_->GetTranslate(); // 編集後の昇降足場座標
    const Math::Vector3 translateDelta = editedTranslate - currentTranslate_; // 移動範囲全体へ加える移動量
    scale_ = object_->GetScale();
    lowerTranslate_ += translateDelta;
    upperTranslate_ += translateDelta;
    currentTranslate_ += translateDelta;
    previousTranslate_ += translateDelta;
}

/// <summary>
/// 保存された表示Transformと昇降範囲を復元する。
/// </summary>
void MovingPlatformGimmick::RestoreEditorTransform(const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& lowerTranslate, const Math::Vector3& upperTranslate)
{
    if (!object_) {
        return;
    }

    scale_ = scale;
    lowerTranslate_ = lowerTranslate;
    upperTranslate_ = upperTranslate;
    currentTranslate_ = lowerTranslate_;
    previousTranslate_ = lowerTranslate_;
    endpointWaitRemainingSeconds_ = 0.0f;
    movingToUpper_ = true;
    object_->SetScale(scale_);
    object_->SetRotate(rotate);
    object_->SetTranslate(currentTranslate_);
}

/// <summary>
/// 入力状態と経過時間から足場位置を更新する。
/// </summary>
void MovingPlatformGimmick::Update(float deltaTime, bool shouldMove)
{
    previousTranslate_ = currentTranslate_;
    active_ = shouldMove;
    if (active_) {
        const float safeDeltaTime = (std::max)(deltaTime, 0.0f); // タイマーと移動に使用する負数を除いた経過時間
        if (endpointWaitRemainingSeconds_ > 0.0f) {
            endpointWaitRemainingSeconds_ = (std::max)(endpointWaitRemainingSeconds_ - safeDeltaTime, 0.0f);
            if (endpointWaitRemainingSeconds_ > 0.0f) {
                ApplyVisual();
                return;
            }
            movingToUpper_ = !movingToUpper_;
        }

        const Math::Vector3& targetTranslate = movingToUpper_ ? upperTranslate_ : lowerTranslate_; // 現在向かう端点座標
        currentTranslate_ = MoveTowards(currentTranslate_, targetTranslate, moveSpeed_ * safeDeltaTime);
        const Math::Vector3 remaining = {
            targetTranslate.x - currentTranslate_.x,
            targetTranslate.y - currentTranslate_.y,
            targetTranslate.z - currentTranslate_.z
        }; // 更新後に残っている目標までの距離
        const float remainingDistanceSquared = remaining.x * remaining.x + remaining.y * remaining.y + remaining.z * remaining.z; // 端点到達判定用の距離二乗
        if (remainingDistanceSquared <= 0.000001f) {
            const float endpointWaitSeconds = movingToUpper_ ? upperWaitSeconds_ : lowerWaitSeconds_; // 到達した端点で停止する秒数
            if (endpointWaitSeconds > 0.0f) {
                endpointWaitRemainingSeconds_ = endpointWaitSeconds;
            } else {
                movingToUpper_ = !movingToUpper_;
            }
        }
    } else {
        endpointWaitRemainingSeconds_ = 0.0f;
        movingToUpper_ = true;
        currentTranslate_ = MoveTowards(currentTranslate_, lowerTranslate_, moveSpeed_ * (std::max)(deltaTime, 0.0f));
    }
    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void MovingPlatformGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 昇降足場を描画する。
/// </summary>
void MovingPlatformGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

/// <summary>
/// 昇降足場を下端の停止状態へ戻す。
/// </summary>
void MovingPlatformGimmick::Reset()
{
    currentTranslate_ = lowerTranslate_;
    previousTranslate_ = lowerTranslate_;
    active_ = false;
    movingToUpper_ = true;
    endpointWaitRemainingSeconds_ = 0.0f;
    ApplyVisual();
}

/// <summary>
/// 現在位置の全面コライダーを取得する。
/// </summary>
SolidCollider MovingPlatformGimmick::GetSolidCollider() const
{
    SolidCollider collider {}; // 現在位置から作成する足場コライダー
    collider.center = currentTranslate_;
    collider.halfSize = CalculateBoxHalfSize(scale_);
    collider.enabled = true;
    return collider;
}

/// <summary>
/// 更新前位置の全面コライダーを取得する。
/// </summary>
SolidCollider MovingPlatformGimmick::GetPreviousSolidCollider() const
{
    SolidCollider collider {}; // 更新前位置から作成する足場コライダー
    collider.center = previousTranslate_;
    collider.halfSize = CalculateBoxHalfSize(scale_);
    collider.enabled = true;
    return collider;
}

/// <summary>
/// 直近の更新で移動した量を取得する。
/// </summary>
Math::Vector3 MovingPlatformGimmick::GetMovementDelta() const
{
    return {
        currentTranslate_.x - previousTranslate_.x,
        currentTranslate_.y - previousTranslate_.y,
        currentTranslate_.z - previousTranslate_.z
    };
}

/// <summary>
/// 現在状態に応じた表示色と座標を反映する。
/// </summary>
void MovingPlatformGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    object_->SetTranslate(currentTranslate_);
    object_->SetMaterialColor(active_ ? activeColor_ : inactiveColor_);
}

/// <summary>
/// 一方通行ゲートの表示、衝突範囲、通行方向を初期化する。
/// </summary>
void OneWayGateGimmick::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const OneWayGateGimmickDesc& desc)
{
    scale_ = desc.scale;
    translate_ = desc.translate;
    passableColor_ = desc.passableColor;
    blockingColor_ = desc.blockingColor;
    allowedDirectionX_ = desc.allowedDirectionX >= 0.0f ? 1.0f : -1.0f;
    object_ = CreateGimmickObject(object3dCommon, imguiManager, desc.objectId, desc.modelFileName, scale_, translate_, passableColor_);
    Reset();
}

/// <summary>
/// 一方通行ゲートが保持する表示用リソースを解放する。
/// </summary>
void OneWayGateGimmick::Finalize()
{
    object_.reset();
    Reset();
}

/// <summary>
/// 表示オブジェクトの編集結果をゲートの衝突判定へ反映する。
/// </summary>
void OneWayGateGimmick::ApplyEditorTransform()
{
    if (!object_) {
        return;
    }
    scale_ = object_->GetScale();
    translate_ = object_->GetTranslate();
}

/// <summary>
/// プレイヤー位置からゲートの遮断状態を更新する。
/// </summary>
void OneWayGateGimmick::Update(const PlayerState& playerState)
{
    if (blocking_) {
        ApplyVisual();
        return;
    }

    const float signedDistance = (playerState.transform.translate.x - translate_.x) * allowedDirectionX_; // 許可方向を正としたゲートからの距離
    blocking_ = signedDistance > 0.0f;
    ApplyVisual();
}

/// <summary>
/// 表示用オブジェクトを更新する。
/// </summary>
void OneWayGateGimmick::UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!object_) {
        return;
    }

    object_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 一方通行ゲートを描画する。
/// </summary>
void OneWayGateGimmick::Draw()
{
    if (!object_) {
        return;
    }

    if (blocking_) {
        object_->Draw();
        return;
    }

    DrawWithAlphaBlend();
}

/// <summary>
/// ゲート状態を初期状態へ戻す。
/// </summary>
void OneWayGateGimmick::Reset()
{
    blocking_ = false;
    ApplyVisual();
}

/// <summary>
/// 現在のプレイヤー位置で有効な全面コライダーを取得する。
/// </summary>
SolidCollider OneWayGateGimmick::GetSolidCollider() const
{
    SolidCollider collider {}; // ゲートから作成する全面コライダー
    collider.center = translate_;
    collider.halfSize = CalculateBoxHalfSize(scale_);
    collider.enabled = blocking_;
    return collider;
}

/// <summary>
/// 現在状態に応じた表示色を反映する。
/// </summary>
void OneWayGateGimmick::ApplyVisual()
{
    if (!object_) {
        return;
    }

    const Math::Vector4 color = blocking_ ? blockingColor_ : passableColor_; // 遮断状態に応じた表示色
    object_->SetMaterialColor(color);
}

/// <summary>
/// 指定した3DオブジェクトをAlphaブレンドで描画する。
/// </summary>
void OneWayGateGimmick::DrawWithAlphaBlend()
{
    DrawObjectWithAlphaBlend(object_.get());
}

/// <summary>
/// ゴール判定範囲を設定する。
/// </summary>
void BoxGoalGimmick::Configure(const BoxGoalGimmickDesc& desc)
{
    center_ = desc.center;
    halfSize_ = desc.halfSize;
    reached_ = false;
}

/// <summary>
/// プレイヤー状態からゴール到達を更新する。
/// </summary>
bool BoxGoalGimmick::Update(const PlayerState& playerState)
{
    if (reached_) {
        return true;
    }

    reached_ = Contains(playerState.transform.translate);
    return reached_;
}

/// <summary>
/// ゴール到達状態を初期状態へ戻す。
/// </summary>
void BoxGoalGimmick::Reset()
{
    reached_ = false;
}

/// <summary>
/// ゴール表示の編集量を判定範囲へ反映する。
/// </summary>
void BoxGoalGimmick::ApplyEditorTransform(const Math::Vector3& translateDelta, const Math::Vector3& scaleRatio)
{
    center_ += translateDelta;
    halfSize_ = {
        halfSize_.x * scaleRatio.x,
        halfSize_.y * scaleRatio.y,
        halfSize_.z * scaleRatio.z,
    };
}

/// <summary>
/// 指定位置がゴール判定範囲内にあるか判定する。
/// </summary>
bool BoxGoalGimmick::Contains(const Math::Vector3& position) const
{
    return IsPointInsideBox(position, center_, halfSize_);
}
