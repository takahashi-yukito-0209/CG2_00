#include "PlayScene.h"

#include "ImGuiManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/io/InputManager.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

using namespace MyEngine;

namespace {
constexpr const char* kPlayerPrototypeModelFileName = "block/block.obj"; // 確認用プレイヤーに使用する仮モデル
constexpr float kPlayerPrototypeCameraDistance = 30.0f; // 2.5D確認用カメラの見た目距離
constexpr Math::Vector3 kPlayerPrototypeCameraRotate = { -0.12f, 0.0f, 0.0f }; // 横視点に少し見下ろしを足した確認用カメラ回転
constexpr float kPlayerPrototypeCameraFovY = 0.55f; // 2.5D確認用カメラ視野角
constexpr Math::Vector3 kPlayerPrototypeCameraFocusOffset = { 0.0f, 2.2f, 0.0f }; // プレイヤーを画面内に収める注視点補正
constexpr uint8_t kRecordToggleKey = DIK_C; // 分身用記録の開始・停止キー
constexpr uint8_t kClonePlayKey = DIK_V; // 分身再生キー
constexpr uint8_t kCloneStopKey = DIK_B; // 分身停止キー
constexpr uint8_t kPrototypeResetKey = DIK_R; // 確認用パズルのリセットキー
constexpr float kPlayerPrototypeFallResetY = -5.0f; // 仮ステージ外へ落ちたとみなすY座標
constexpr Math::Vector3 kPlayerPrototypeStartTranslate = { -8.7f, 0.5f, 0.0f }; // プレイヤー開始位置
constexpr Math::Vector4 kPlayerPrototypeNormalPlayerColor = { 0.35f, 0.8f, 1.0f, 1.0f }; // 通常時のプレイヤー色
constexpr Math::Vector4 kPlayerPrototypeRecordingPlayerColor = { 1.0f, 0.35f, 0.85f, 1.0f }; // 記録中のプレイヤー色
constexpr Math::Vector4 kPlayerPrototypeClearPlayerColor = { 1.0f, 0.95f, 0.25f, 1.0f }; // クリア時のプレイヤー色
constexpr Math::Vector3 kPlayerPrototypeCloneStartMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身開始地点マーカーの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeCloneEndMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身終了地点マーカーの表示サイズ
constexpr Math::Vector4 kPlayerPrototypeCloneStartMarkerColor = { 0.35f, 0.8f, 1.0f, 0.3f }; // 分身開始地点マーカーの表示色
constexpr Math::Vector4 kPlayerPrototypeCloneEndMarkerColor = { 1.0f, 0.35f, 0.85f, 0.32f }; // 分身終了地点マーカーの表示色

struct PlayerPrototypeStageBlockDesc {
    Math::Vector3 scale; // 仮ブロックの大きさ
    Math::Vector3 translate; // 仮ブロックの中心位置
    Math::Vector4 color; // 仮ブロックの表示色
    bool collidable; // 全面コライダーとして使うか
    bool goalMarker; // ゴール表示用のブロックか
};

constexpr std::array<PlayerPrototypeStageBlockDesc, 8> kPlayerPrototypeStageBlockDescs = { {
    { { 4.6f, 0.1f, 4.0f }, { -7.2f, -0.05f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, true, false },
    { { 2.4f, 0.1f, 4.0f }, { -3.4f, -0.05f, 0.0f }, { 0.92f, 0.96f, 1.0f, 1.0f }, true, false },
    { { 1.2f, 0.2f, 3.0f }, { -1.25f, 0.1f, 0.0f }, { 0.65f, 0.85f, 1.0f, 1.0f }, true, false },
    { { 0.9f, 0.04f, 3.2f }, { -1.25f, 0.26f, 0.0f }, { 0.25f, 1.0f, 1.0f, 1.0f }, false, false },
    { { 3.2f, 0.1f, 4.0f }, { 1.4f, 2.05f, 0.0f }, { 0.75f, 1.0f, 0.8f, 1.0f }, true, false },
    { { 2.8f, 0.1f, 4.0f }, { 5.0f, 2.05f, 0.0f }, { 0.75f, 1.0f, 0.8f, 1.0f }, true, false },
    { { 1.6f, 0.1f, 4.0f }, { 7.2f, 2.55f, 0.0f }, { 0.62f, 0.9f, 0.82f, 1.0f }, true, false },
    { { 0.2f, 2.0f, 2.0f }, { 8.0f, 3.6f, 0.0f }, { 0.35f, 1.0f, 0.55f, 1.0f }, false, true },
} }; // 分身の踏み台化、スイッチ維持、扉通過を順に確認する仮ステージブロック
constexpr Math::Vector3 kPlayerPrototypeGoalCenter = { 8.0f, 3.6f, 0.0f }; // 仮ゴール判定の中心
constexpr Math::Vector3 kPlayerPrototypeGoalHalfSize = { 1.0f, 1.0f, 1.2f }; // 仮ゴール判定の半サイズ
constexpr Math::Vector3 kPlayerPrototypeSwitchScale = { 2.0f, 0.12f, 2.5f }; // 仮スイッチの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeSwitchTranslate = { 1.2f, 2.16f, 0.0f }; // 仮スイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeSwitchVolumeCenter = { 1.2f, 2.68f, 0.0f }; // スイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPlayerPrototypeSwitchVolumeHalfSize = { 1.2f, 0.75f, 1.5f }; // スイッチ入力を受ける範囲半サイズ
constexpr Math::Vector3 kPlayerPrototypeDoorScale = { 0.3f, 2.7f, 3.0f }; // 仮扉の表示サイズ
constexpr Math::Vector3 kPlayerPrototypeDoorTranslate = { 3.25f, 3.45f, 0.0f }; // 仮扉の中心座標
constexpr Math::Vector4 kPlayerPrototypeSwitchInactiveColor = { 0.25f, 0.25f, 0.28f, 1.0f }; // 押されていない仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeSwitchActiveColor = { 0.1f, 1.0f, 0.45f, 1.0f }; // 押されている仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeDoorClosedColor = { 1.0f, 0.25f, 0.25f, 1.0f }; // 閉じている仮扉色
constexpr Math::Vector4 kPlayerPrototypeDoorOpenColor = { 0.1f, 1.0f, 0.45f, 0.25f }; // 開いている仮扉色
constexpr Math::Vector4 kPlayerPrototypeGoalClearColor = { 1.0f, 0.95f, 0.25f, 1.0f }; // クリア済みの仮ゴール色

/// <summary>
/// 仮ステージブロックの半サイズを計算する。
/// </summary>
Math::Vector3 CalculateStageBlockHalfSize(const Math::Vector3& scale)
{
    return {
        std::fabs(scale.x) * 0.5f,
        std::fabs(scale.y) * 0.5f,
        std::fabs(scale.z) * 0.5f
    };
}

/// <summary>
/// 仮ステージブロックから全面コライダー情報を作成する。
/// </summary>
SolidCollider BuildStageBlockSolidCollider(const PlayerPrototypeStageBlockDesc& blockDesc)
{
    SolidCollider collider {}; // 仮ステージから作成する全面コライダー
    collider.center = blockDesc.translate;
    collider.halfSize = CalculateStageBlockHalfSize(blockDesc.scale);
    collider.enabled = blockDesc.collidable;
    return collider;
}

/// <summary>
/// 閉じている仮扉の全面コライダー情報を作成する。
/// </summary>
SolidCollider BuildPlayerPrototypeDoorCollider()
{
    SolidCollider collider {}; // 仮扉から作成する全面コライダー
    collider.center = kPlayerPrototypeDoorTranslate;
    collider.halfSize = CalculateStageBlockHalfSize(kPlayerPrototypeDoorScale);
    collider.enabled = true;
    return collider;
}

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
/// 仮ギミック用のブロックオブジェクトを作成する。
/// </summary>
std::unique_ptr<Object3d> CreatePlayerPrototypeBlockObject(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, uint32_t objectId, const Math::Vector3& scale, const Math::Vector3& translate, const Math::Vector4& color)
{
    std::unique_ptr<Object3d> object = std::make_unique<Object3d>(); // 作成する仮ブロックオブジェクト
    object->SetObjectId(objectId);
    object->Initialize(object3dCommon, imguiManager);
    object->SetModel(kPlayerPrototypeModelFileName);
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
/// プレイヤー状態が指定した箱形範囲内にあるか判定する。
/// </summary>
bool IsPlayerStateInsideBox(const PlayerState& state, const Math::Vector3& center, const Math::Vector3& halfSize)
{
    return IsPointInsideBox(state.transform.translate, center, halfSize);
}

/// <summary>
/// ImGui操作中にプレイヤー移動入力を止める必要があるか判定する。
/// </summary>
bool ShouldBlockPlayerInput()
{
#ifdef USE_IMGUI
    if (!ImGui::GetCurrentContext()) {
        return false;
    }

    const ImGuiIO& imguiIo = ImGui::GetIO(); // ImGuiの入力取得状態
    return imguiIo.WantCaptureKeyboard || ImGui::IsAnyItemActive();
#else
    return false;
#endif
}

/// <summary>
/// プレイヤー位置からカメラ注視点を計算する。
/// </summary>
Math::Vector3 CalculatePlayerPrototypeCameraFocus(const PlayerState& playerState)
{
    Math::Vector3 focus = playerState.transform.translate; // カメラ中心にするプレイヤー座標
    focus.x += kPlayerPrototypeCameraFocusOffset.x;
    focus.y += kPlayerPrototypeCameraFocusOffset.y;
    focus.z = kPlayerPrototypeCameraFocusOffset.z;
    return focus;
}

/// <summary>
/// 確認用カメラの回転を反映した座標を計算する。
/// </summary>
Math::Vector3 RotatePointForPlayerPrototypeCamera(const Math::Vector3& position)
{
    const float cosX = std::cos(kPlayerPrototypeCameraRotate.x); // X回転のcos値
    const float sinX = std::sin(kPlayerPrototypeCameraRotate.x); // X回転のsin値
    return {
        position.x,
        position.y * cosX - position.z * sinX,
        position.y * sinX + position.z * cosX
    };
}

/// <summary>
/// 2.5D用の横視点カメラをプレイヤー位置に合わせて設定する。
/// </summary>
void ConfigurePlayerPrototypeCamera(Camera* camera, const Math::Vector3& focus)
{
    if (!camera) {
        return;
    }

    const Math::Vector3 rotatedFocus = RotatePointForPlayerPrototypeCamera(focus); // ビュー回転後の注視点座標
    const Math::Vector3 cameraTranslate = {
        rotatedFocus.x,
        rotatedFocus.y,
        rotatedFocus.z - kPlayerPrototypeCameraDistance
    }; // 横視点で注視点を画面中央に置くカメラ位置
    camera->SetTranslate(cameraTranslate);
    camera->SetRotate(kPlayerPrototypeCameraRotate);
    camera->SetFovY(kPlayerPrototypeCameraFovY);
    camera->Update();
}

/// <summary>
/// プレイヤーが上面に乗れる足場一覧を作成する。
/// </summary>
std::vector<StandablePlatform> BuildPlayerStandablePlatforms(const PastSelfClone& pastSelfClone)
{
    std::vector<StandablePlatform> platforms; // プレイヤー用の上面足場一覧
    const StandablePlatform clonePlatform = pastSelfClone.GetStandablePlatform(); // 分身の上面足場
    if (clonePlatform.enabled) {
        platforms.push_back(clonePlatform);
    }
    return platforms;
}

/// <summary>
/// 分身が上面に乗れる足場一覧を作成する。
/// </summary>
std::vector<StandablePlatform> BuildCloneStandablePlatforms(const Player& player)
{
    std::vector<StandablePlatform> platforms; // 分身用の上面足場一覧
    const StandablePlatform playerPlatform = player.GetStandablePlatform(); // プレイヤーの上面足場
    if (playerPlatform.enabled) {
        platforms.push_back(playerPlatform);
    }
    return platforms;
}
} // namespace

/// <summary>
/// プレイヤー確認用オブジェクトを初期化する。
/// </summary>
void PlayScene::InitializePlayerPrototype()
{
    InitializePlayerPrototypeStage();
    InitializePlayerPrototypeMechanics();
    player_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, kPlayerPrototypeModelFileName);
    player_.SetMaterialColor(kPlayerPrototypeNormalPlayerColor);
    PlayerState playerStartState = player_.GetState(); // 仮ステージに合わせた開始状態
    playerStartState.transform.translate = kPlayerPrototypeStartTranslate;
    player_.SetInitialState(playerStartState);
    pastSelfClone_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, kPlayerPrototypeModelFileName);
    ConfigurePlayerPrototypeCamera(ctx_.camera, CalculatePlayerPrototypeCameraFocus(player_.GetState()));
}

/// <summary>
/// プレイヤー確認用の仮ステージを初期化する。
/// </summary>
void PlayScene::InitializePlayerPrototypeStage()
{
    playerPrototypeStageBlocks_.clear();
    playerPrototypeStageBlocks_.reserve(kPlayerPrototypeStageBlockDescs.size());

    for (const PlayerPrototypeStageBlockDesc& blockDesc : kPlayerPrototypeStageBlockDescs) {
        PlayerPrototypeStageBlock stageBlock {}; // 生成する仮ステージブロック
        stageBlock.object = std::make_unique<Object3d>();
        stageBlock.object->SetObjectId(IssueObjectId());
        stageBlock.object->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        stageBlock.object->SetModel(kPlayerPrototypeModelFileName);
        stageBlock.object->SetScale(blockDesc.scale);
        stageBlock.object->SetRotate({ 0.0f, 0.0f, 0.0f });
        stageBlock.object->SetTranslate(blockDesc.translate);
        stageBlock.object->SetMaterialColor(blockDesc.color);
        stageBlock.object->SetUseTexture(false);
        stageBlock.object->SetEnableLighting(false);
        stageBlock.object->SetUseAlphaDiscard(false);
        stageBlock.collider = BuildStageBlockSolidCollider(blockDesc);
        stageBlock.goalMarker = blockDesc.goalMarker;
        playerPrototypeStageBlocks_.push_back(std::move(stageBlock));
    }

    playerPrototypeGoalReached_ = false;
    ApplyPlayerPrototypeGoalVisual();
}

/// <summary>
/// プレイヤー確認用の分身ギミックを初期化する。
/// </summary>
void PlayScene::InitializePlayerPrototypeMechanics()
{
    playerPrototypeSwitchObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeSwitchScale, kPlayerPrototypeSwitchTranslate, kPlayerPrototypeSwitchInactiveColor);
    playerPrototypeDoorObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeDoorScale, kPlayerPrototypeDoorTranslate, kPlayerPrototypeDoorClosedColor);
    playerPrototypeCloneStartMarkerObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeCloneStartMarkerScale, kPlayerPrototypeStartTranslate, kPlayerPrototypeCloneStartMarkerColor);
    playerPrototypeCloneEndMarkerObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeCloneEndMarkerScale, kPlayerPrototypeStartTranslate, kPlayerPrototypeCloneEndMarkerColor);
    playerPrototypeSwitchActive_ = false;
    playerPrototypeDoorOpen_ = false;
}

/// <summary>
/// 分身用スイッチが押されているか判定する。
/// </summary>
bool PlayScene::IsPlayerPrototypeSwitchPressed() const
{
    if (IsPlayerStateInsideBox(player_.GetState(), kPlayerPrototypeSwitchVolumeCenter, kPlayerPrototypeSwitchVolumeHalfSize)) {
        return true;
    }

    return pastSelfClone_.IsVisible() && IsPlayerStateInsideBox(pastSelfClone_.GetCurrentState(), kPlayerPrototypeSwitchVolumeCenter, kPlayerPrototypeSwitchVolumeHalfSize);
}

/// <summary>
/// プレイヤー確認用の分身ギミックを更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeMechanics()
{
    playerPrototypeSwitchActive_ = IsPlayerPrototypeSwitchPressed();
    playerPrototypeDoorOpen_ = playerPrototypeSwitchActive_;

    if (playerPrototypeSwitchObject_) {
        const Math::Vector4 switchColor = playerPrototypeSwitchActive_ ? kPlayerPrototypeSwitchActiveColor : kPlayerPrototypeSwitchInactiveColor; // 現在状態に応じた仮スイッチ色
        playerPrototypeSwitchObject_->SetMaterialColor(switchColor);
    }
    if (playerPrototypeDoorObject_) {
        const Math::Vector4 doorColor = playerPrototypeDoorOpen_ ? kPlayerPrototypeDoorOpenColor : kPlayerPrototypeDoorClosedColor; // 現在状態に応じた仮扉色
        playerPrototypeDoorObject_->SetMaterialColor(doorColor);
    }
}

/// <summary>
/// プレイヤー確認用状態を初期状態へ戻す。
/// </summary>
void PlayScene::ResetPlayerPrototypeState()
{
    player_.Reset();
    pastSelfRecorder_.Clear();
    pastSelfClone_.Stop();
    playerPrototypeGoalReached_ = false;
    playerPrototypeSwitchActive_ = false;
    playerPrototypeDoorOpen_ = false;
    player_.SetMaterialColor(kPlayerPrototypeNormalPlayerColor);
    UpdatePlayerPrototypeMechanics();
    ApplyPlayerPrototypeGoalVisual();
}

/// <summary>
/// プレイヤー確認用の分身ギミック表示を更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeMechanicObjects(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (playerPrototypeSwitchObject_) {
        playerPrototypeSwitchObject_->Update(viewMatrix, projectionMatrix);
    }
    if (playerPrototypeDoorObject_) {
        playerPrototypeDoorObject_->Update(viewMatrix, projectionMatrix);
    }
}

/// <summary>
/// 分身記録の開始・終了地点マーカーを更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeCloneRecordMarkers(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!playerPrototypeCloneStartMarkerObject_ || !playerPrototypeCloneEndMarkerObject_) {
        return;
    }

    const std::vector<PastSelfFrame>& frames = pastSelfRecorder_.GetFrames(); // 記録済みの分身フレーム
    if (frames.empty() || pastSelfRecorder_.IsRecording()) {
        return;
    }

    const PlayerState& startState = frames.front().state; // 分身の開始位置として使う最初の状態
    playerPrototypeCloneStartMarkerObject_->SetScale(kPlayerPrototypeCloneStartMarkerScale);
    playerPrototypeCloneStartMarkerObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    playerPrototypeCloneStartMarkerObject_->SetTranslate(startState.transform.translate);
    playerPrototypeCloneStartMarkerObject_->Update(viewMatrix, projectionMatrix);

    const PlayerState& endState = frames.back().state; // 分身の終了位置として使う最後の状態
    playerPrototypeCloneEndMarkerObject_->SetScale(kPlayerPrototypeCloneEndMarkerScale);
    playerPrototypeCloneEndMarkerObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    playerPrototypeCloneEndMarkerObject_->SetTranslate(endState.transform.translate);
    playerPrototypeCloneEndMarkerObject_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 分身記録の開始・終了地点マーカーを描画する。
/// </summary>
void PlayScene::DrawPlayerPrototypeCloneRecordMarkers()
{
    if (!playerPrototypeCloneStartMarkerObject_ || !playerPrototypeCloneEndMarkerObject_) {
        return;
    }

    const std::vector<PastSelfFrame>& frames = pastSelfRecorder_.GetFrames(); // 表示判定に使う記録済みフレーム
    if (frames.empty() || pastSelfRecorder_.IsRecording()) {
        return;
    }

    DrawObjectWithAlphaBlend(playerPrototypeCloneStartMarkerObject_.get());
    DrawObjectWithAlphaBlend(playerPrototypeCloneEndMarkerObject_.get());
}

/// <summary>
/// プレイヤー確認用の分身ギミックを描画する。
/// </summary>
void PlayScene::DrawPlayerPrototypeMechanics()
{
    if (playerPrototypeSwitchObject_) {
        playerPrototypeSwitchObject_->Draw();
    }
    if (!playerPrototypeDoorObject_) {
        return;
    }
    if (!playerPrototypeDoorOpen_) {
        playerPrototypeDoorObject_->Draw();
        return;
    }

    DrawObjectWithAlphaBlend(playerPrototypeDoorObject_.get());
}

/// <summary>
/// プレイヤー確認用状態を更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototype(float deltaTime)
{
    const bool blockInputByImGui = ShouldBlockPlayerInput(); // ImGui操作でゲーム入力を止めるか
    InputManager* inputManager = InputManager::GetInstance(); // プレイヤー確認用入力を取得する管理クラス
    if (!blockInputByImGui && inputManager && inputManager->IsKeyJustPressed(kPrototypeResetKey)) {
        ResetPlayerPrototypeState();
    }

    const bool canAcceptInput = !playerPrototypeGoalReached_ && !blockInputByImGui; // プレイヤーと分身操作の入力を受け取れるか
    if (canAcceptInput && inputManager) {
        if (inputManager->IsKeyJustPressed(kRecordToggleKey)) {
            pastSelfRecorder_.Toggle();
        }
        if (inputManager->IsKeyJustPressed(kClonePlayKey)) {
            pastSelfClone_.Start(pastSelfRecorder_.GetFrames());
        }
        if (inputManager->IsKeyJustPressed(kCloneStopKey)) {
            pastSelfClone_.Stop();
        }
    }

    if (!playerPrototypeGoalReached_) {
        pastSelfClone_.Update(deltaTime, BuildCloneStandablePlatforms(player_));
        UpdatePlayerPrototypeMechanics();
        std::vector<SolidCollider> solidColliders; // プレイヤーが全面衝突する地形と扉
        AppendPlayerPrototypeSolidColliders(&solidColliders);
        std::vector<StandablePlatform> standablePlatforms = BuildPlayerStandablePlatforms(pastSelfClone_); // プレイヤーが上面だけ乗れる分身足場
        player_.Update(deltaTime, canAcceptInput, solidColliders, standablePlatforms);
        if (player_.GetState().transform.translate.y < kPlayerPrototypeFallResetY) {
            ResetPlayerPrototypeState();
        }
        pastSelfRecorder_.Update(deltaTime, player_.GetState());
        const Math::Vector4 playerColor = pastSelfRecorder_.IsRecording() ? kPlayerPrototypeRecordingPlayerColor : kPlayerPrototypeNormalPlayerColor; // 記録状態に応じたプレイヤー色
        player_.SetMaterialColor(playerColor);
        UpdatePlayerPrototypeMechanics();
        UpdatePlayerPrototypeGoal();
    } else {
        player_.SetMaterialColor(kPlayerPrototypeClearPlayerColor);
    }

    ConfigurePlayerPrototypeCamera(ctx_.camera, CalculatePlayerPrototypeCameraFocus(player_.GetState()));

    if (!ctx_.camera) {
        return;
    }

    const Math::Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // プレイヤー更新に使用するビュー行列
    const Math::Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // プレイヤー更新に使用する射影行列
    UpdatePlayerPrototypeStage(viewMatrix, projectionMatrix);
    UpdatePlayerPrototypeMechanicObjects(viewMatrix, projectionMatrix);
    UpdatePlayerPrototypeCloneRecordMarkers(viewMatrix, projectionMatrix);
    player_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfClone_.UpdateObject(viewMatrix, projectionMatrix);
}

/// <summary>
/// プレイヤー確認用の仮ステージを更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeStage(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    for (PlayerPrototypeStageBlock& stageBlock : playerPrototypeStageBlocks_) {
        if (stageBlock.object) {
            stageBlock.object->Update(viewMatrix, projectionMatrix);
        }
    }
}

/// <summary>
/// プレイヤー確認用の仮ステージを描画する。
/// </summary>
void PlayScene::DrawPlayerPrototypeStage()
{
    for (PlayerPrototypeStageBlock& stageBlock : playerPrototypeStageBlocks_) {
        if (stageBlock.object) {
            stageBlock.object->Draw();
        }
    }
}

/// <summary>
/// プレイヤー確認用の全面コライダーを追加する。
/// </summary>
void PlayScene::AppendPlayerPrototypeSolidColliders(std::vector<SolidCollider>* colliders) const
{
    if (!colliders) {
        return;
    }

    for (const PlayerPrototypeStageBlock& stageBlock : playerPrototypeStageBlocks_) {
        if (stageBlock.collider.enabled) {
            colliders->push_back(stageBlock.collider);
        }
    }

    if (!playerPrototypeDoorOpen_) {
        colliders->push_back(BuildPlayerPrototypeDoorCollider());
    }
}

/// <summary>
/// プレイヤー確認用ゴール判定を更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeGoal()
{
    if (playerPrototypeGoalReached_) {
        return;
    }

    const Math::Vector3& playerPosition = player_.GetState().transform.translate; // 判定に使うプレイヤー中心座標
    if (!IsPointInsideBox(playerPosition, kPlayerPrototypeGoalCenter, kPlayerPrototypeGoalHalfSize)) {
        return;
    }

    playerPrototypeGoalReached_ = true;
    pastSelfRecorder_.Stop();
    pastSelfClone_.Stop();
    player_.SetMaterialColor(kPlayerPrototypeClearPlayerColor);
    ApplyPlayerPrototypeGoalVisual();
}

/// <summary>
/// プレイヤー確認用ゴール表示を現在状態に合わせる。
/// </summary>
void PlayScene::ApplyPlayerPrototypeGoalVisual()
{
    for (PlayerPrototypeStageBlock& stageBlock : playerPrototypeStageBlocks_) {
        if (!stageBlock.object || !stageBlock.goalMarker) {
            continue;
        }

        const Math::Vector4 goalColor = playerPrototypeGoalReached_ ? kPlayerPrototypeGoalClearColor : kPlayerPrototypeStageBlockDescs.back().color; // 現在状態に応じたゴール色
        stageBlock.object->SetMaterialColor(goalColor);
    }
}

/// <summary>
/// プレイヤー確認用オブジェクトを描画する。
/// </summary>
void PlayScene::DrawPlayerPrototype()
{
    DrawPlayerPrototypeStage();
    DrawPlayerPrototypeMechanics();
    DrawPlayerPrototypeCloneRecordMarkers();
    pastSelfClone_.Draw();
    player_.Draw();
}

/// <summary>
/// ImGuiでプレイヤー確認用の状態を表示する。
/// </summary>
void PlayScene::DrawPlayerPrototypeImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Move: A/D or Left Stick X");
    ImGui::Text("Jump: Space or GamePad A");
    ImGui::Text("Record: C  Play Clone: V  Stop Clone: B  Reset: R");
    ImGui::Text("Switch: %s", playerPrototypeSwitchActive_ ? "ON" : "OFF");
    ImGui::Text("Door: %s", playerPrototypeDoorOpen_ ? "Open" : "Closed");
    if (playerPrototypeGoalReached_) {
        ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.25f, 1.0f), "CLEAR");
    }
    ImGui::Text("Goal: %s", playerPrototypeGoalReached_ ? "Reached" : "Not Reached");
    if (playerPrototypeGoalReached_) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(pastSelfRecorder_.IsRecording() ? "Stop Recording" : "Start Recording")) {
        pastSelfRecorder_.Toggle();
    }
    ImGui::SameLine();
    if (ImGui::Button("Play Clone")) {
        pastSelfClone_.Start(pastSelfRecorder_.GetFrames());
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Clone")) {
        pastSelfClone_.Stop();
    }
    if (playerPrototypeGoalReached_) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Puzzle")) {
        ResetPlayerPrototypeState();
    }
    if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
        player_.DrawImGui();
    }
    if (ImGui::CollapsingHeader("Recorder", ImGuiTreeNodeFlags_DefaultOpen)) {
        pastSelfRecorder_.DrawImGui();
    }
    if (ImGui::CollapsingHeader("Clone", ImGuiTreeNodeFlags_DefaultOpen)) {
        pastSelfClone_.DrawImGui();
    }
#endif
}
