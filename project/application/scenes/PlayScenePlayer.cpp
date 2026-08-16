#include "PlayScene.h"

#include "ImGuiManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/io/InputManager.h"

#include <algorithm>
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
constexpr Math::Vector4 kPlayerPrototypeNormalPlayerColor = { 0.18f, 0.62f, 1.0f, 1.0f }; // 通常時のプレイヤー色
constexpr Math::Vector4 kPlayerPrototypeRecordingPlayerColor = { 1.0f, 0.18f, 0.75f, 1.0f }; // 記録中のプレイヤー色
constexpr Math::Vector4 kPlayerPrototypeClearPlayerColor = { 1.0f, 0.88f, 0.12f, 1.0f }; // クリア時のプレイヤー色
constexpr Math::Vector3 kPlayerPrototypeCloneStartMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身開始地点マーカーの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeCloneEndMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身終了地点マーカーの表示サイズ
constexpr Math::Vector4 kPlayerPrototypeCloneStartMarkerColor = { 0.18f, 0.62f, 1.0f, 0.38f }; // 分身開始地点マーカーの表示色
constexpr Math::Vector4 kPlayerPrototypeCloneEndMarkerColor = { 1.0f, 0.18f, 0.75f, 0.42f }; // 分身終了地点マーカーの表示色

struct PlayerPrototypeStageBlockDesc {
    Math::Vector3 scale; // 仮ブロックの大きさ
    Math::Vector3 translate; // 仮ブロックの中心位置
    Math::Vector4 color; // 仮ブロックの表示色
    bool collidable; // 全面コライダーとして使うか
    bool goalMarker; // ゴール表示用のブロックか
};

constexpr std::array<PlayerPrototypeStageBlockDesc, 8> kPlayerPrototypeStageBlockDescs = { {
    { { 4.8f, 0.12f, 4.0f }, { -7.1f, -0.06f, 0.0f }, { 0.92f, 0.96f, 1.0f, 1.0f }, true, false },
    { { 2.6f, 0.12f, 4.0f }, { -3.45f, -0.06f, 0.0f }, { 0.82f, 0.92f, 1.0f, 1.0f }, true, false },
    { { 1.25f, 0.34f, 3.3f }, { -1.45f, 0.17f, 0.0f }, { 0.55f, 0.82f, 1.0f, 1.0f }, true, false },
    { { 1.1f, 0.05f, 3.4f }, { 1.2f, 1.33f, 0.0f }, { 0.1f, 1.0f, 0.9f, 1.0f }, false, false },
    { { 3.2f, 0.12f, 4.0f }, { 1.2f, 1.19f, 0.0f }, { 0.55f, 0.95f, 0.72f, 1.0f }, true, false },
    { { 2.9f, 0.12f, 4.0f }, { 5.1f, 2.59f, 0.0f }, { 0.72f, 0.98f, 0.68f, 1.0f }, true, false },
    { { 2.0f, 0.12f, 4.0f }, { 7.65f, 2.94f, 0.0f }, { 0.64f, 0.92f, 0.72f, 1.0f }, true, false },
    { { 0.22f, 2.2f, 2.2f }, { 8.45f, 4.0f, 0.0f }, { 0.12f, 1.0f, 0.45f, 1.0f }, false, true },
} }; // 分身の踏み台化、スイッチ維持、扉通過を順に確認する仮ステージブロック
constexpr Math::Vector3 kPlayerPrototypeGoalCenter = { 8.45f, 4.0f, 0.0f }; // 仮ゴール判定の中心
constexpr Math::Vector3 kPlayerPrototypeGoalHalfSize = { 1.05f, 1.15f, 1.25f }; // 仮ゴール判定の半サイズ
constexpr Math::Vector3 kPlayerPrototypeSwitchScale = { 2.0f, 0.12f, 2.7f }; // 仮スイッチの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeSwitchTranslate = { 1.2f, 1.31f, 0.0f }; // 仮スイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeSwitchVolumeCenter = { 1.2f, 1.82f, 0.0f }; // スイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPlayerPrototypeSwitchVolumeHalfSize = { 1.15f, 0.72f, 1.55f }; // スイッチ入力を受ける範囲半サイズ
constexpr Math::Vector3 kPlayerPrototypeDoorScale = { 0.35f, 2.9f, 3.1f }; // 仮扉の表示サイズ
constexpr Math::Vector3 kPlayerPrototypeDoorTranslate = { 3.25f, 2.75f, 0.0f }; // 仮扉の中心座標
constexpr Math::Vector4 kPlayerPrototypeSwitchInactiveColor = { 0.18f, 0.18f, 0.22f, 1.0f }; // 押されていない仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeSwitchActiveColor = { 0.0f, 1.0f, 0.45f, 1.0f }; // 押されている仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeSwitchPlayerOnlyColor = { 1.0f, 0.62f, 0.12f, 1.0f }; // プレイヤーだけが乗っている仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeDoorClosedColor = { 1.0f, 0.12f, 0.12f, 1.0f }; // 閉じている仮扉色
constexpr Math::Vector4 kPlayerPrototypeDoorOpenColor = { 0.0f, 1.0f, 0.45f, 0.22f }; // 開いている仮扉色
constexpr Math::Vector4 kPlayerPrototypeGoalClearColor = { 1.0f, 0.88f, 0.12f, 1.0f }; // クリア済みの仮ゴール色
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchScale = { 1.7f, 0.12f, 2.5f }; // 時間差スイッチの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchTranslate = { 5.1f, 2.71f, 0.0f }; // 時間差スイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchVolumeCenter = { 5.1f, 3.21f, 0.0f }; // 時間差スイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchVolumeHalfSize = { 1.0f, 0.7f, 1.45f }; // 時間差スイッチ入力を受ける範囲半サイズ
constexpr Math::Vector3 kPlayerPrototypeTimedDoorScale = { 0.3f, 2.2f, 3.0f }; // 時間差扉の表示サイズ
constexpr Math::Vector3 kPlayerPrototypeTimedDoorTranslate = { 6.55f, 3.72f, 0.0f }; // 時間差扉の中心座標
constexpr Math::Vector4 kPlayerPrototypeTimedSwitchInactiveColor = { 0.12f, 0.18f, 0.22f, 1.0f }; // 時間差スイッチ未入力時の表示色
constexpr Math::Vector4 kPlayerPrototypeTimedSwitchActiveColor = { 0.0f, 0.7f, 1.0f, 1.0f }; // 時間差スイッチ起動中の表示色
constexpr Math::Vector4 kPlayerPrototypeTimedSwitchTriggerColor = { 0.2f, 1.0f, 0.85f, 1.0f }; // 時間差スイッチを分身が踏んでいる時の表示色
constexpr Math::Vector4 kPlayerPrototypeTimedDoorClosedColor = { 0.0f, 0.38f, 1.0f, 1.0f }; // 閉じている時間差扉色
constexpr Math::Vector4 kPlayerPrototypeTimedDoorOpenColor = { 0.0f, 0.7f, 1.0f, 0.22f }; // 開いている時間差扉色
constexpr float kPlayerPrototypeTimedSwitchHoldSeconds = 3.0f; // 時間差スイッチの起動維持秒数
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchScale = { 1.45f, 0.12f, 2.4f }; // 重さスイッチの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchTranslate = { 7.65f, 3.06f, 0.0f }; // 重さスイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchVolumeCenter = { 7.65f, 3.56f, 0.0f }; // 重さスイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchVolumeHalfSize = { 0.95f, 0.7f, 1.45f }; // 重さスイッチ入力を受ける範囲半サイズ
constexpr Math::Vector4 kPlayerPrototypeWeightSwitchInactiveColor = { 0.18f, 0.18f, 0.22f, 1.0f }; // 重さスイッチ未入力時の表示色
constexpr Math::Vector4 kPlayerPrototypeWeightSwitchPartialColor = { 1.0f, 0.62f, 0.12f, 1.0f }; // 重さスイッチ片方入力時の表示色
constexpr Math::Vector4 kPlayerPrototypeWeightSwitchActiveColor = { 0.65f, 1.0f, 0.1f, 1.0f }; // 重さスイッチ両方入力時の表示色
constexpr Math::Vector3 kPlayerPrototypeOneWayGateScale = { 0.24f, 1.8f, 2.8f }; // 一方通行ゲートの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeOneWayGateTranslate = { 8.05f, 3.9f, 0.0f }; // 終盤で戻りを塞ぐ一方通行ゲートの中心座標
constexpr Math::Vector4 kPlayerPrototypeOneWayGatePassableColor = { 0.0f, 0.65f, 1.0f, 0.25f }; // 通行可能時の一方通行ゲート色
constexpr Math::Vector4 kPlayerPrototypeOneWayGateBlockingColor = { 0.15f, 0.25f, 1.0f, 0.85f }; // 戻りを塞ぐ時の一方通行ゲート色

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
/// プレイヤー状態のTransformから半サイズを計算する。
/// </summary>
Math::Vector3 CalculatePlayerStateHalfSize(const PlayerState& state)
{
    return {
        std::fabs(state.transform.scale.x) * 0.5f,
        std::fabs(state.transform.scale.y) * 0.5f,
        std::fabs(state.transform.scale.z) * 0.5f
    };
}

/// <summary>
/// プレイヤー状態と全面コライダーの範囲が重なっているか判定する。
/// </summary>
bool IsPlayerStateTouchingSolidCollider(const PlayerState& state, const SolidCollider& collider)
{
    if (!collider.enabled) {
        return false;
    }

    constexpr float kTouchTolerance = 0.04f; // 接触状態を拾うための余白
    const Math::Vector3 playerHalfSize = CalculatePlayerStateHalfSize(state); // 判定に使うプレイヤー半サイズ
    const bool overlapsX = std::fabs(state.transform.translate.x - collider.center.x) <= playerHalfSize.x + collider.halfSize.x + kTouchTolerance; // X方向の接触
    const bool overlapsY = std::fabs(state.transform.translate.y - collider.center.y) <= playerHalfSize.y + collider.halfSize.y + kTouchTolerance; // Y方向の接触
    const bool overlapsZ = std::fabs(state.transform.translate.z - collider.center.z) <= playerHalfSize.z + collider.halfSize.z + kTouchTolerance; // Z方向の接触
    return overlapsX && overlapsY && overlapsZ;
}

/// <summary>
/// プレイヤー状態が指定された足場の上に立っているか判定する。
/// </summary>
bool IsPlayerStateStandingOnPlatform(const PlayerState& state, const StandablePlatform& platform)
{
    if (!platform.enabled || !state.isGrounded) {
        return false;
    }

    constexpr float kStandingTolerance = 0.08f; // 足元と足場上面のずれ許容値
    constexpr float kPlatformHorizontalInset = 0.02f; // 端での誤判定を避ける内側余白
    const Math::Vector3 playerHalfSize = CalculatePlayerStateHalfSize(state); // 判定に使うプレイヤー半サイズ
    const float playerFootY = state.transform.translate.y - playerHalfSize.y; // プレイヤー足元Y座標
    const float platformTopY = platform.center.y + platform.halfSize.y; // 足場上面Y座標
    const float platformHalfX = (std::max)(platform.halfSize.x - kPlatformHorizontalInset, 0.0f); // 判定に使う足場X半幅
    const float platformHalfZ = (std::max)(platform.halfSize.z - kPlatformHorizontalInset, 0.0f); // 判定に使う足場Z半幅
    const bool overlapsX = std::fabs(state.transform.translate.x - platform.center.x) <= playerHalfSize.x + platformHalfX; // X方向の重なり
    const bool overlapsZ = std::fabs(state.transform.translate.z - platform.center.z) <= playerHalfSize.z + platformHalfZ; // Z方向の重なり
    const bool touchesTop = std::fabs(playerFootY - platformTopY) <= kStandingTolerance; // 上面に接しているか
    return overlapsX && overlapsZ && touchesTop;
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
    BoxSwitchGimmickDesc switchDesc {}; // 分身専用スイッチの初期化情報
    switchDesc.objectId = IssueObjectId();
    switchDesc.modelFileName = kPlayerPrototypeModelFileName;
    switchDesc.scale = kPlayerPrototypeSwitchScale;
    switchDesc.translate = kPlayerPrototypeSwitchTranslate;
    switchDesc.volumeCenter = kPlayerPrototypeSwitchVolumeCenter;
    switchDesc.volumeHalfSize = kPlayerPrototypeSwitchVolumeHalfSize;
    switchDesc.inactiveColor = kPlayerPrototypeSwitchInactiveColor;
    switchDesc.activeColor = kPlayerPrototypeSwitchActiveColor;
    switchDesc.playerOnlyColor = kPlayerPrototypeSwitchPlayerOnlyColor;

    LinkedDoorGimmickDesc doorDesc {}; // スイッチ連動扉の初期化情報
    doorDesc.objectId = IssueObjectId();
    doorDesc.modelFileName = kPlayerPrototypeModelFileName;
    doorDesc.scale = kPlayerPrototypeDoorScale;
    doorDesc.translate = kPlayerPrototypeDoorTranslate;
    doorDesc.closedColor = kPlayerPrototypeDoorClosedColor;
    doorDesc.openColor = kPlayerPrototypeDoorOpenColor;

    BoxGoalGimmickDesc goalDesc {}; // ゴール判定の初期化情報
    goalDesc.center = kPlayerPrototypeGoalCenter;
    goalDesc.halfSize = kPlayerPrototypeGoalHalfSize;

    TimedSwitchGimmickDesc timedSwitchDesc {}; // 時間差スイッチの初期化情報
    timedSwitchDesc.objectId = IssueObjectId();
    timedSwitchDesc.modelFileName = kPlayerPrototypeModelFileName;
    timedSwitchDesc.scale = kPlayerPrototypeTimedSwitchScale;
    timedSwitchDesc.translate = kPlayerPrototypeTimedSwitchTranslate;
    timedSwitchDesc.volumeCenter = kPlayerPrototypeTimedSwitchVolumeCenter;
    timedSwitchDesc.volumeHalfSize = kPlayerPrototypeTimedSwitchVolumeHalfSize;
    timedSwitchDesc.inactiveColor = kPlayerPrototypeTimedSwitchInactiveColor;
    timedSwitchDesc.activeColor = kPlayerPrototypeTimedSwitchActiveColor;
    timedSwitchDesc.triggerColor = kPlayerPrototypeTimedSwitchTriggerColor;
    timedSwitchDesc.holdSeconds = kPlayerPrototypeTimedSwitchHoldSeconds;

    LinkedDoorGimmickDesc timedDoorDesc {}; // 時間差扉の初期化情報
    timedDoorDesc.objectId = IssueObjectId();
    timedDoorDesc.modelFileName = kPlayerPrototypeModelFileName;
    timedDoorDesc.scale = kPlayerPrototypeTimedDoorScale;
    timedDoorDesc.translate = kPlayerPrototypeTimedDoorTranslate;
    timedDoorDesc.closedColor = kPlayerPrototypeTimedDoorClosedColor;
    timedDoorDesc.openColor = kPlayerPrototypeTimedDoorOpenColor;

    WeightSwitchGimmickDesc weightSwitchDesc {}; // 重さスイッチの初期化情報
    weightSwitchDesc.objectId = IssueObjectId();
    weightSwitchDesc.modelFileName = kPlayerPrototypeModelFileName;
    weightSwitchDesc.scale = kPlayerPrototypeWeightSwitchScale;
    weightSwitchDesc.translate = kPlayerPrototypeWeightSwitchTranslate;
    weightSwitchDesc.volumeCenter = kPlayerPrototypeWeightSwitchVolumeCenter;
    weightSwitchDesc.volumeHalfSize = kPlayerPrototypeWeightSwitchVolumeHalfSize;
    weightSwitchDesc.inactiveColor = kPlayerPrototypeWeightSwitchInactiveColor;
    weightSwitchDesc.partialColor = kPlayerPrototypeWeightSwitchPartialColor;
    weightSwitchDesc.activeColor = kPlayerPrototypeWeightSwitchActiveColor;

    OneWayGateGimmickDesc oneWayGateDesc {}; // 一方通行ゲートの初期化情報
    oneWayGateDesc.objectId = IssueObjectId();
    oneWayGateDesc.modelFileName = kPlayerPrototypeModelFileName;
    oneWayGateDesc.scale = kPlayerPrototypeOneWayGateScale;
    oneWayGateDesc.translate = kPlayerPrototypeOneWayGateTranslate;
    oneWayGateDesc.passableColor = kPlayerPrototypeOneWayGatePassableColor;
    oneWayGateDesc.blockingColor = kPlayerPrototypeOneWayGateBlockingColor;
    oneWayGateDesc.allowedDirectionX = 1.0f;

    playerPrototypeSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, switchDesc);
    playerPrototypeDoor_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, doorDesc);
    playerPrototypeTimedSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, timedSwitchDesc);
    playerPrototypeTimedDoor_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, timedDoorDesc);
    playerPrototypeWeightSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, weightSwitchDesc);
    playerPrototypeOneWayGate_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, oneWayGateDesc);
    playerPrototypeGoal_.Configure(goalDesc);
    playerPrototypeCloneStartMarkerObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeCloneStartMarkerScale, kPlayerPrototypeStartTranslate, kPlayerPrototypeCloneStartMarkerColor);
    playerPrototypeCloneEndMarkerObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeCloneEndMarkerScale, kPlayerPrototypeStartTranslate, kPlayerPrototypeCloneEndMarkerColor);
    playerPrototypeSwitchActive_ = false;
    playerPrototypeDoorOpen_ = false;
    playerPrototypePlayerOnSwitch_ = false;
    playerPrototypeCloneOnSwitch_ = false;
    playerPrototypeDoorBlockedBeforeClone_ = false;
    playerPrototypeClonePlatformUsed_ = false;
    playerPrototypeDoorOpenedByClone_ = false;
    playerPrototypeTimedSwitchActive_ = false;
    playerPrototypeTimedSwitchCloneOn_ = false;
    playerPrototypeTimedDoorOpen_ = false;
    playerPrototypeWeightSwitchActive_ = false;
    playerPrototypeWeightPlayerOn_ = false;
    playerPrototypeWeightCloneOn_ = false;
    playerPrototypeOneWayGateBlocking_ = false;
    playerPrototypeTimedDoorOpened_ = false;
    playerPrototypeWeightSwitchActivated_ = false;
    playerPrototypeOneWayGateUsed_ = false;
    playerPrototypeElapsedTime_ = 0.0f;
    playerPrototypeClearTime_ = 0.0f;
    playerPrototypeLastRecordDuration_ = 0.0f;
}

/// <summary>
/// プレイヤー確認用の分身ギミックを更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeMechanics(float deltaTime)
{
    const bool cloneVisible = pastSelfClone_.IsVisible(); // 分身ギミック判定に使う分身表示状態
    const PlayerState& cloneState = pastSelfClone_.GetCurrentState(); // 分身ギミック判定に使う現在状態
    playerPrototypeSwitch_.Update(player_.GetState(), cloneVisible, cloneState);
    playerPrototypePlayerOnSwitch_ = playerPrototypeSwitch_.IsPlayerOnSwitch();
    playerPrototypeCloneOnSwitch_ = playerPrototypeSwitch_.IsCloneOnSwitch();
    playerPrototypeSwitchActive_ = playerPrototypeSwitch_.IsActive();
    playerPrototypeDoor_.Update(playerPrototypeSwitchActive_);
    playerPrototypeDoorOpen_ = playerPrototypeDoor_.IsOpen();

    playerPrototypeTimedSwitch_.Update(deltaTime, cloneVisible, cloneState);
    playerPrototypeTimedSwitchActive_ = playerPrototypeTimedSwitch_.IsActive();
    playerPrototypeTimedSwitchCloneOn_ = playerPrototypeTimedSwitch_.IsCloneOnSwitch();
    playerPrototypeWeightSwitch_.Update(player_.GetState(), cloneVisible, cloneState);
    playerPrototypeWeightSwitchActive_ = playerPrototypeWeightSwitch_.IsActive();
    playerPrototypeWeightPlayerOn_ = playerPrototypeWeightSwitch_.IsPlayerOnSwitch();
    playerPrototypeWeightCloneOn_ = playerPrototypeWeightSwitch_.IsCloneOnSwitch();
    playerPrototypeTimedDoor_.Update(playerPrototypeTimedSwitchActive_ || playerPrototypeWeightSwitchActive_);
    playerPrototypeTimedDoorOpen_ = playerPrototypeTimedDoor_.IsOpen();
    playerPrototypeOneWayGate_.Update(player_.GetState());
    playerPrototypeOneWayGateBlocking_ = playerPrototypeOneWayGate_.IsBlocking();

    if (playerPrototypeCloneOnSwitch_) {
        playerPrototypeDoorOpenedByClone_ = true;
    }
    if (playerPrototypeTimedDoorOpen_) {
        playerPrototypeTimedDoorOpened_ = true;
    }
    if (playerPrototypeWeightSwitchActive_) {
        playerPrototypeWeightSwitchActivated_ = true;
    }
    if (playerPrototypeOneWayGateBlocking_) {
        playerPrototypeOneWayGateUsed_ = true;
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
    playerPrototypeGoal_.Reset();
    playerPrototypeSwitch_.Reset();
    playerPrototypeDoor_.Reset();
    playerPrototypeTimedSwitch_.Reset();
    playerPrototypeTimedDoor_.Reset();
    playerPrototypeWeightSwitch_.Reset();
    playerPrototypeOneWayGate_.Reset();
    playerPrototypeGoalReached_ = false;
    playerPrototypeSwitchActive_ = false;
    playerPrototypeDoorOpen_ = false;
    playerPrototypePlayerOnSwitch_ = false;
    playerPrototypeCloneOnSwitch_ = false;
    playerPrototypeDoorBlockedBeforeClone_ = false;
    playerPrototypeClonePlatformUsed_ = false;
    playerPrototypeDoorOpenedByClone_ = false;
    playerPrototypeTimedSwitchActive_ = false;
    playerPrototypeTimedSwitchCloneOn_ = false;
    playerPrototypeTimedDoorOpen_ = false;
    playerPrototypeWeightSwitchActive_ = false;
    playerPrototypeWeightPlayerOn_ = false;
    playerPrototypeWeightCloneOn_ = false;
    playerPrototypeOneWayGateBlocking_ = false;
    playerPrototypeTimedDoorOpened_ = false;
    playerPrototypeWeightSwitchActivated_ = false;
    playerPrototypeOneWayGateUsed_ = false;
    playerPrototypeElapsedTime_ = 0.0f;
    playerPrototypeClearTime_ = 0.0f;
    playerPrototypeLastRecordDuration_ = 0.0f;
    player_.SetMaterialColor(kPlayerPrototypeNormalPlayerColor);
    UpdatePlayerPrototypeMechanics(0.0f);
    ApplyPlayerPrototypeGoalVisual();
}

/// <summary>
/// プレイヤー確認用の分身ギミック表示を更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeMechanicObjects(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    playerPrototypeSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeDoor_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeTimedSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeTimedDoor_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeWeightSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeOneWayGate_.UpdateObject(viewMatrix, projectionMatrix);
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
    playerPrototypeSwitch_.Draw();
    playerPrototypeDoor_.Draw();
    playerPrototypeTimedSwitch_.Draw();
    playerPrototypeTimedDoor_.Draw();
    playerPrototypeWeightSwitch_.Draw();
    playerPrototypeOneWayGate_.Draw();
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
            const bool wasRecording = pastSelfRecorder_.IsRecording(); // 切り替え前に記録中だったか
            pastSelfRecorder_.Toggle();
            if (wasRecording) {
                playerPrototypeLastRecordDuration_ = pastSelfRecorder_.GetDuration();
            } else {
                playerPrototypeLastRecordDuration_ = 0.0f;
            }
        }
        if (inputManager->IsKeyJustPressed(kClonePlayKey)) {
            pastSelfClone_.Start(pastSelfRecorder_.GetFrames());
        }
        if (inputManager->IsKeyJustPressed(kCloneStopKey)) {
            pastSelfClone_.Stop();
        }
    }

    if (!playerPrototypeGoalReached_) {
        playerPrototypeElapsedTime_ += deltaTime;
        pastSelfClone_.Update(deltaTime, BuildCloneStandablePlatforms(player_));
        UpdatePlayerPrototypeMechanics(deltaTime);
        std::vector<SolidCollider> solidColliders; // プレイヤーが全面衝突する地形と扉
        AppendPlayerPrototypeSolidColliders(&solidColliders);
        std::vector<StandablePlatform> standablePlatforms = BuildPlayerStandablePlatforms(pastSelfClone_); // プレイヤーが上面だけ乗れる分身足場
        player_.Update(deltaTime, canAcceptInput, solidColliders, standablePlatforms);
        const SolidCollider doorCollider = playerPrototypeDoor_.GetSolidCollider(); // 閉じている扉の衝突判定
        if (doorCollider.enabled && IsPlayerStateTouchingSolidCollider(player_.GetState(), doorCollider)) {
            playerPrototypeDoorBlockedBeforeClone_ = true;
        }
        if (IsPlayerStateStandingOnPlatform(player_.GetState(), pastSelfClone_.GetStandablePlatform())) {
            playerPrototypeClonePlatformUsed_ = true;
        }
        if (player_.GetState().transform.translate.y < kPlayerPrototypeFallResetY) {
            ResetPlayerPrototypeState();
        }
        pastSelfRecorder_.Update(deltaTime, player_.GetState());
        if (!pastSelfRecorder_.IsRecording() && pastSelfRecorder_.GetFrames().size() >= 2) {
            playerPrototypeLastRecordDuration_ = pastSelfRecorder_.GetDuration();
        }
        const Math::Vector4 playerColor = pastSelfRecorder_.IsRecording() ? kPlayerPrototypeRecordingPlayerColor : kPlayerPrototypeNormalPlayerColor; // 記録状態に応じたプレイヤー色
        player_.SetMaterialColor(playerColor);
        UpdatePlayerPrototypeMechanics(0.0f);
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

    const SolidCollider doorCollider = playerPrototypeDoor_.GetSolidCollider(); // 閉じている扉の全面コライダー
    if (doorCollider.enabled) {
        colliders->push_back(doorCollider);
    }

    const SolidCollider timedDoorCollider = playerPrototypeTimedDoor_.GetSolidCollider(); // 閉じている時間差扉の全面コライダー
    if (timedDoorCollider.enabled) {
        colliders->push_back(timedDoorCollider);
    }

    const SolidCollider oneWayGateCollider = playerPrototypeOneWayGate_.GetSolidCollider(); // 戻り方向を塞ぐ一方通行ゲートの全面コライダー
    if (oneWayGateCollider.enabled) {
        colliders->push_back(oneWayGateCollider);
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

    if (!playerPrototypeGoal_.Update(player_.GetState())) {
        return;
    }

    playerPrototypeGoalReached_ = true;
    playerPrototypeClearTime_ = playerPrototypeElapsedTime_;
    playerPrototypeLastRecordDuration_ = pastSelfRecorder_.GetDuration();
    pastSelfRecorder_.Stop();
    pastSelfClone_.Pause();
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
    ImGui::Text("Switch Source: Clone %s / Player %s", playerPrototypeCloneOnSwitch_ ? "ON" : "OFF", playerPrototypePlayerOnSwitch_ ? "ON" : "OFF");
    ImGui::Text("Door: %s", playerPrototypeDoorOpen_ ? "Open" : "Closed");
    ImGui::Text("Timed: Switch %s %.2f sec / Door %s", playerPrototypeTimedSwitchActive_ ? "ON" : "OFF", playerPrototypeTimedSwitch_.GetRemainingSeconds(), playerPrototypeTimedDoorOpen_ ? "Open" : "Closed");
    ImGui::Text("Weight: %s  Player %s / Clone %s", playerPrototypeWeightSwitchActive_ ? "ON" : "OFF", playerPrototypeWeightPlayerOn_ ? "ON" : "OFF", playerPrototypeWeightCloneOn_ ? "ON" : "OFF");
    ImGui::Text("OneWay: %s", playerPrototypeOneWayGateBlocking_ ? "Blocking" : "Passable");
    ImGui::Text("Time: %.2f sec  Clear: %.2f sec  Record: %.2f sec", playerPrototypeElapsedTime_, playerPrototypeClearTime_, playerPrototypeLastRecordDuration_);
    if (playerPrototypeGoalReached_) {
        ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.25f, 1.0f), "CLEAR");
    }
    ImGui::Text("Goal: %s", playerPrototypeGoalReached_ ? "Reached" : "Not Reached");
    if (playerPrototypeGoalReached_) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(pastSelfRecorder_.IsRecording() ? "Stop Recording" : "Start Recording")) {
        const bool wasRecording = pastSelfRecorder_.IsRecording(); // 切り替え前に記録中だったか
        pastSelfRecorder_.Toggle();
        playerPrototypeLastRecordDuration_ = wasRecording ? pastSelfRecorder_.GetDuration() : 0.0f;
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

/// <summary>
/// プレイヤー確認用の検証状態をPlayerタブ内に表示する。
/// </summary>
void PlayScene::DrawPlayerPrototypeStatusHud()
{
#ifdef USE_IMGUI
    const ImVec4 checkedColor = ImVec4(0.15f, 1.0f, 0.45f, 1.0f); // 達成済み項目の表示色
    const ImVec4 uncheckedColor = ImVec4(1.0f, 0.35f, 0.25f, 1.0f); // 未達成項目の表示色
    const char* recordStateLabel = pastSelfRecorder_.IsRecording() ? "Recording" : "Stopped"; // 記録状態表示
    const char* cloneStateLabel = !pastSelfClone_.IsVisible() ? "Stopped" : (pastSelfClone_.IsPlaying() ? "Playing" : "Finished"); // 分身状態表示
    const bool hasRecordFrames = pastSelfRecorder_.GetFrames().size() >= 2; // 分身再生に使える記録があるか
    const char* nextActionText = "Pass the door and reach the goal"; // HUDに表示する次の確認手順
    if (playerPrototypeGoalReached_) {
        nextActionText = "Record this clear state in one take";
    } else if (!playerPrototypeDoorBlockedBeforeClone_) {
        nextActionText = "First, show the closed door blocks the player";
    } else if (pastSelfRecorder_.IsRecording()) {
        nextActionText = "Move to the clone-only switch, then press C";
    } else if (!hasRecordFrames) {
        nextActionText = "Press C and record a route to the switch";
    } else if (!pastSelfClone_.IsVisible()) {
        nextActionText = "Press V to play the recorded clone";
    } else if (playerPrototypePlayerOnSwitch_ && !playerPrototypeCloneOnSwitch_) {
        nextActionText = "Player contact is ignored; wait for clone";
    } else if (!playerPrototypeDoorOpenedByClone_) {
        nextActionText = "Wait until the clone opens the switch";
    } else if (!playerPrototypeClonePlatformUsed_) {
        nextActionText = "Use the clone as a platform to reach the high floor";
    } else if (!playerPrototypeTimedDoorOpened_) {
        nextActionText = "Record/play a second clone route to the timed switch";
    } else if (!playerPrototypeWeightSwitchActivated_) {
        nextActionText = "Follow the clone and stand on the weight switch together";
    } else if (!playerPrototypeOneWayGateUsed_) {
        nextActionText = "Pass the one-way gate, then try to return briefly";
    }

    ImGui::Text("Prototype Verify");
    ImGui::Separator();
    ImGui::Text("Move A/D  Jump Space  Record C  Play V  Stop B  Reset R");
    ImGui::Text("Time  : %.2f sec  Clear %.2f sec", playerPrototypeElapsedTime_, playerPrototypeClearTime_);
    ImGui::Text("Record: %s  Frames: %zu  %.2f sec", recordStateLabel, pastSelfRecorder_.GetFrames().size(), playerPrototypeLastRecordDuration_);
    ImGui::Text("Clone : %s  %.2f / %.2f sec", cloneStateLabel, pastSelfClone_.GetPlaybackTime(), pastSelfClone_.GetDuration());
    ImGui::Text("Switch: %s  Clone %s  Player %s",
        playerPrototypeSwitchActive_ ? "ON" : "OFF",
        playerPrototypeCloneOnSwitch_ ? "ON" : "OFF",
        playerPrototypePlayerOnSwitch_ ? "ON" : "OFF");
    ImGui::Text("Door  : %s  Timed %s  Goal %s%s", playerPrototypeDoorOpen_ ? "Open" : "Closed", playerPrototypeTimedDoorOpen_ ? "Open" : "Closed", playerPrototypeGoalReached_ ? "Reached" : "Not Reached", playerPrototypeGoalReached_ ? " / CLEAR" : "");
    ImGui::Text("Timed: %s  Clone %s  %.2f sec", playerPrototypeTimedSwitchActive_ ? "ON" : "OFF", playerPrototypeTimedSwitchCloneOn_ ? "ON" : "OFF", playerPrototypeTimedSwitch_.GetRemainingSeconds());
    ImGui::Text("Weight: %s  Player %s  Clone %s", playerPrototypeWeightSwitchActive_ ? "ON" : "OFF", playerPrototypeWeightPlayerOn_ ? "ON" : "OFF", playerPrototypeWeightCloneOn_ ? "ON" : "OFF");
    ImGui::Text("OneWay: %s", playerPrototypeOneWayGateBlocking_ ? "Blocking" : "Passable");
    ImGui::Text("Next  : %s", nextActionText);
    ImGui::Separator();
    ImGui::TextColored(playerPrototypeDoorBlockedBeforeClone_ ? checkedColor : uncheckedColor, "[%c] Closed door blocked player", playerPrototypeDoorBlockedBeforeClone_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeDoorOpenedByClone_ ? checkedColor : uncheckedColor, "[%c] Clone opened door switch", playerPrototypeDoorOpenedByClone_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeClonePlatformUsed_ ? checkedColor : uncheckedColor, "[%c] Player used clone as platform", playerPrototypeClonePlatformUsed_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeTimedDoorOpened_ ? checkedColor : uncheckedColor, "[%c] Timed or weight switch opened blue door", playerPrototypeTimedDoorOpened_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeWeightSwitchActivated_ ? checkedColor : uncheckedColor, "[%c] Player and clone activated weight switch", playerPrototypeWeightSwitchActivated_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeOneWayGateUsed_ ? checkedColor : uncheckedColor, "[%c] One-way gate blocked return path", playerPrototypeOneWayGateUsed_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeGoalReached_ ? checkedColor : uncheckedColor, "[%c] Goal reached", playerPrototypeGoalReached_ ? 'x' : ' ');
    if (playerPrototypeGoalReached_) {
        ImGui::Separator();
        ImGui::TextColored(checkedColor, "Goal Reached / CLEAR");
        ImGui::TextColored(checkedColor, "Clear %.2f sec / Record %.2f sec", playerPrototypeClearTime_, playerPrototypeLastRecordDuration_);
    }
#endif
}
