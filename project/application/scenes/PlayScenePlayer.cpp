#include "PlayScene.h"

#include "ImGuiManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/io/InputManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <span>
#include <typeinfo>
#include <vector>

using namespace MyEngine;

namespace {
constexpr const char* kPlayerPrototypeModelFileName = "block/block.obj"; // 確認用プレイヤーに使用する仮モデル
constexpr float kPlayerPrototypeCameraMinimumDistance = 24.0f; // 対象が近い時のカメラ最小距離
constexpr float kPlayerPrototypeCameraMaximumDistance = 44.0f; // 対象が離れた時のカメラ最大距離
constexpr Math::Vector3 kPlayerPrototypeCameraRotate = { -0.12f, 0.0f, 0.0f }; // 横視点に少し見下ろしを足した確認用カメラ回転
constexpr float kPlayerPrototypeCameraFovY = 0.62f; // 2.5D確認用カメラ視野角
constexpr float kPlayerPrototypeCameraVisibleAspect = 1.15f; // 右側HUDを除いたゲーム表示領域として扱う横縦比
constexpr float kPlayerPrototypeCameraHorizontalPadding = 3.5f; // 対象範囲の左右に確保する余白
constexpr float kPlayerPrototypeCameraVerticalPadding = 2.5f; // 対象範囲の上下に確保する余白
constexpr float kPlayerPrototypeCameraFollowSpeed = 6.0f; // 注視点と距離を追従させる速度
constexpr Math::Vector3 kPlayerPrototypeCameraFocusOffset = { 2.0f, 1.5f, 0.0f }; // 右側HUDを避けながら対象を画面内に収める注視点補正
constexpr uint8_t kRecordToggleKey = DIK_C; // 分身用記録の開始・停止キー
constexpr uint8_t kClonePlayKey = DIK_V; // 分身再生キー
constexpr uint8_t kCloneStopKey = DIK_B; // 分身停止キー
constexpr uint8_t kCloneUndoKey = DIK_X; // 最後に保存した分身の削除キー
constexpr uint8_t kReplayPrepareKey = DIK_T; // 記録を残したまま再生準備へ戻すキー
constexpr uint8_t kPrototypeResetKey = DIK_R; // 確認用パズルのリセットキー
constexpr float kPrepareFeedbackDuration = 1.5f; // Prepare成功表示を維持する秒数
constexpr float kCompletedCheckFeedbackDuration = 3.0f; // 検証項目の達成通知を表示する秒数
constexpr float kPlayerPrototypeFallResetY = -5.0f; // 仮ステージ外へ落ちたとみなすY座標
constexpr Math::Vector3 kPlayerPrototypeStartTranslate = { -8.7f, 0.5f, 0.0f }; // プレイヤー開始位置
constexpr Math::Vector4 kPlayerPrototypeNormalPlayerColor = { 0.0f, 0.86f, 1.0f, 1.0f }; // 通常時のプレイヤー色
constexpr Math::Vector4 kPlayerPrototypeRecordingPlayerColor = { 1.0f, 0.22f, 0.02f, 1.0f }; // 記録中のプレイヤー色
constexpr Math::Vector4 kPlayerPrototypeClearPlayerColor = { 1.0f, 0.88f, 0.12f, 1.0f }; // クリア時のプレイヤー色
constexpr Math::Vector3 kPlayerPrototypeCloneStartMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身開始地点マーカーの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeCloneEndMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身終了地点マーカーの表示サイズ
constexpr Math::Vector4 kPlayerPrototypeCloneStartMarkerColor = { 0.0f, 0.86f, 1.0f, 0.5f }; // 分身開始地点マーカーの表示色
constexpr Math::Vector4 kPlayerPrototypeCloneEndMarkerColor = { 1.0f, 0.05f, 0.95f, 0.55f }; // 分身終了地点マーカーの表示色

struct PlayerPrototypeStageBlockDesc {
    Math::Vector3 scale; // 仮ブロックの大きさ
    Math::Vector3 translate; // 仮ブロックの中心位置
    Math::Vector4 color; // 仮ブロックの表示色
    bool collidable; // 全面コライダーとして使うか
    bool goalMarker; // ゴール表示用のブロックか
};

struct PlayerPrototypeCameraFrame {
    Math::Vector3 focus; // カメラが追従する注視点
    float distance; // 対象範囲を収めるカメラ距離
};

constexpr std::array<PlayerPrototypeStageBlockDesc, 11> kPlayerPrototypeStageBlockDescs = { {
    { { 2.8f, 0.12f, 4.0f }, { -15.5f, 2.44f, 0.0f }, { 1.0f, 0.72f, 0.3f, 1.0f }, true, false },
    { { 5.0f, 0.12f, 4.0f }, { -12.0f, -0.06f, 0.0f }, { 0.96f, 0.88f, 0.7f, 1.0f }, true, false },
    { { 4.8f, 0.12f, 4.0f }, { -7.1f, -0.06f, 0.0f }, { 0.92f, 0.96f, 1.0f, 1.0f }, true, false },
    { { 2.6f, 0.12f, 4.0f }, { -3.45f, -0.06f, 0.0f }, { 0.82f, 0.92f, 1.0f, 1.0f }, true, false },
    { { 1.25f, 0.34f, 3.3f }, { -1.45f, 0.17f, 0.0f }, { 0.55f, 0.82f, 1.0f, 1.0f }, true, false },
    { { 1.1f, 0.05f, 3.4f }, { 1.2f, 1.33f, 0.0f }, { 0.1f, 1.0f, 0.9f, 1.0f }, false, false },
    { { 3.2f, 0.12f, 4.0f }, { 1.2f, 1.19f, 0.0f }, { 0.55f, 0.95f, 0.72f, 1.0f }, true, false },
    { { 3.4f, 0.12f, 4.0f }, { 5.2f, 2.19f, 0.0f }, { 0.72f, 0.98f, 0.68f, 1.0f }, true, false },
    { { 2.8f, 0.12f, 4.0f }, { 8.4f, 2.44f, 0.0f }, { 0.64f, 0.92f, 0.72f, 1.0f }, true, false },
    { { 2.8f, 0.12f, 4.0f }, { 11.4f, 2.64f, 0.0f }, { 0.8f, 0.94f, 0.68f, 1.0f }, true, false },
    { { 0.22f, 2.2f, 2.2f }, { 15.55f, 3.7f, 0.0f }, { 0.12f, 1.0f, 0.45f, 1.0f }, false, true },
} }; // 各ギミックの作動状態を動画で読めるように間隔を取った仮ステージブロック
constexpr Math::Vector3 kPlayerPrototypeGoalCenter = { 15.55f, 3.7f, 0.0f }; // 仮ゴール判定の中心
constexpr Math::Vector3 kPlayerPrototypeGoalHalfSize = { 0.75f, 1.0f, 1.25f }; // 仮ゴール判定の半サイズ
constexpr Math::Vector3 kPlayerPrototypeSwitchScale = { 2.0f, 0.12f, 2.7f }; // 仮スイッチの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeSwitchTranslate = { 1.2f, 1.31f, 0.0f }; // 仮スイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeSwitchVolumeCenter = { 1.2f, 1.82f, 0.0f }; // スイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPlayerPrototypeSwitchVolumeHalfSize = { 1.15f, 0.72f, 1.55f }; // スイッチ入力を受ける範囲半サイズ
constexpr Math::Vector3 kPlayerPrototypeDoorScale = { 0.35f, 2.9f, 3.1f }; // 仮扉の表示サイズ
constexpr Math::Vector3 kPlayerPrototypeDoorTranslate = { 3.25f, 2.75f, 0.0f }; // 仮扉の中心座標
constexpr Math::Vector4 kPlayerPrototypeSwitchInactiveColor = { 0.03f, 0.16f, 0.08f, 1.0f }; // 押されていない仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeSwitchActiveColor = { 0.0f, 1.0f, 0.32f, 1.0f }; // 押されている仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeSwitchPlayerOnlyColor = { 1.0f, 0.62f, 0.12f, 1.0f }; // プレイヤーだけが乗っている仮スイッチ色
constexpr Math::Vector4 kPlayerPrototypeDoorClosedColor = { 0.0f, 0.55f, 0.18f, 1.0f }; // 閉じている仮扉色
constexpr Math::Vector4 kPlayerPrototypeDoorOpenColor = { 0.3f, 1.0f, 0.52f, 0.48f }; // 開いている仮扉色
constexpr Math::Vector4 kPlayerPrototypeGoalClearColor = { 1.0f, 0.88f, 0.12f, 1.0f }; // クリア済みの仮ゴール色
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchScale = { 1.7f, 0.12f, 2.5f }; // 時間差スイッチの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchTranslate = { 5.2f, 2.31f, 0.0f }; // 時間差スイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchVolumeCenter = { 5.2f, 2.81f, 0.0f }; // 時間差スイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPlayerPrototypeTimedSwitchVolumeHalfSize = { 1.0f, 0.7f, 1.45f }; // 時間差スイッチ入力を受ける範囲半サイズ
constexpr Math::Vector3 kPlayerPrototypeTimedDoorScale = { 0.3f, 2.2f, 3.0f }; // 時間差扉の表示サイズ
constexpr Math::Vector3 kPlayerPrototypeTimedDoorTranslate = { 7.0f, 3.52f, 0.0f }; // 時間差扉の中心座標
constexpr Math::Vector4 kPlayerPrototypeTimedSwitchInactiveColor = { 0.02f, 0.1f, 0.32f, 1.0f }; // 時間差スイッチ未入力時の表示色
constexpr Math::Vector4 kPlayerPrototypeTimedSwitchActiveColor = { 0.0f, 0.82f, 1.0f, 1.0f }; // 時間差スイッチ起動中の表示色
constexpr Math::Vector4 kPlayerPrototypeTimedSwitchTriggerColor = { 0.2f, 1.0f, 1.0f, 1.0f }; // 時間差スイッチを分身が踏んでいる時の表示色
constexpr Math::Vector4 kPlayerPrototypeTimedDoorClosedColor = { 0.0f, 0.16f, 0.8f, 1.0f }; // 閉じている時間差扉色
constexpr Math::Vector4 kPlayerPrototypeTimedDoorOpenColor = { 0.3f, 0.86f, 1.0f, 0.5f }; // 開いている時間差扉色
constexpr float kPlayerPrototypeTimedSwitchHoldSeconds = 4.5f; // 時間差スイッチの起動維持秒数
constexpr Math::Vector3 kPlayerPrototypeToggleSwitchScale = { 1.2f, 0.12f, 2.4f }; // 任意試作区間のトグルスイッチ表示サイズ
constexpr Math::Vector3 kPlayerPrototypeToggleSwitchTranslate = { -7.75f, 0.06f, 0.0f }; // トグルスイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeToggleSwitchVolumeCenter = { -7.75f, 0.56f, 0.0f }; // トグルスイッチ入力範囲の中心
constexpr Math::Vector3 kPlayerPrototypeToggleSwitchVolumeHalfSize = { 0.65f, 0.7f, 1.4f }; // トグルスイッチ入力範囲の半サイズ
constexpr Math::Vector4 kPlayerPrototypeToggleSwitchInactiveColor = { 0.3f, 0.12f, 0.02f, 1.0f }; // トグルスイッチOFF時の表示色
constexpr Math::Vector4 kPlayerPrototypeToggleSwitchActiveColor = { 1.0f, 0.42f, 0.05f, 1.0f }; // トグルスイッチON時の表示色
constexpr Math::Vector4 kPlayerPrototypeToggleSwitchPressedColor = { 1.0f, 0.85f, 0.15f, 1.0f }; // トグルスイッチを分身が踏んでいる時の表示色
constexpr Math::Vector3 kPlayerPrototypeToggleGateScale = { 0.3f, 2.2f, 3.0f }; // 任意試作区間のトグル連動ゲート表示サイズ
constexpr Math::Vector3 kPlayerPrototypeToggleGateTranslate = { -11.5f, 1.1f, 0.0f }; // トグル連動ゲートの中心座標
constexpr Math::Vector4 kPlayerPrototypeToggleGateClosedColor = { 0.55f, 0.18f, 0.02f, 1.0f }; // トグル連動ゲート閉鎖時の表示色
constexpr Math::Vector4 kPlayerPrototypeToggleGateOpenColor = { 1.0f, 0.55f, 0.12f, 0.35f }; // トグル連動ゲート開放時の表示色
constexpr Math::Vector3 kPlayerPrototypeToggleElevatorScale = { 1.8f, 0.08f, 3.0f }; // 任意試作区間の昇降足場表示サイズ
constexpr Math::Vector3 kPlayerPrototypeToggleElevatorLowerTranslate = { -13.2f, 0.04f, 0.0f }; // 昇降足場の下端座標
constexpr Math::Vector3 kPlayerPrototypeToggleElevatorUpperTranslate = { -13.2f, 2.46f, 0.0f }; // 昇降足場の上端座標
constexpr Math::Vector4 kPlayerPrototypeToggleElevatorInactiveColor = { 0.45f, 0.18f, 0.02f, 1.0f }; // 昇降足場停止時の表示色
constexpr Math::Vector4 kPlayerPrototypeToggleElevatorActiveColor = { 1.0f, 0.55f, 0.08f, 1.0f }; // 昇降足場稼働時の表示色
constexpr float kPlayerPrototypeToggleElevatorMoveSpeed = 1.2f; // 昇降足場の1秒あたりの移動距離
constexpr float kPlayerPrototypeToggleElevatorUpperWaitSeconds = 1.0f; // 昇降足場が上端で停止する秒数
constexpr float kPlayerPrototypeToggleElevatorLowerWaitSeconds = 1.0f; // 昇降足場が下端で停止する秒数
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchScale = { 1.45f, 0.12f, 2.4f }; // 重さスイッチの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchTranslate = { 11.4f, 2.76f, 0.0f }; // 重さスイッチの中心座標
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchVolumeCenter = { 11.4f, 3.26f, 0.0f }; // 重さスイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPlayerPrototypeWeightSwitchVolumeHalfSize = { 0.95f, 0.7f, 1.45f }; // 重さスイッチ入力を受ける範囲半サイズ
constexpr Math::Vector4 kPlayerPrototypeWeightSwitchInactiveColor = { 0.32f, 0.22f, 0.04f, 1.0f }; // 重さスイッチ未入力時の表示色
constexpr Math::Vector4 kPlayerPrototypeWeightSwitchPartialColor = { 1.0f, 0.62f, 0.12f, 1.0f }; // 重さスイッチ片方入力時の表示色
constexpr Math::Vector4 kPlayerPrototypeWeightSwitchActiveColor = { 1.0f, 0.9f, 0.12f, 1.0f }; // 重さスイッチ両方入力時の表示色
constexpr Math::Vector3 kPlayerPrototypeGoalBridgeScale = { 3.0f, 0.12f, 4.0f }; // ゴール前の連動橋の表示サイズ
constexpr Math::Vector3 kPlayerPrototypeGoalBridgeTranslate = { 14.2f, 2.64f, 0.0f }; // ゴール前の連動橋の中心座標
constexpr Math::Vector4 kPlayerPrototypeGoalBridgeRetractedColor = { 0.32f, 0.22f, 0.04f, 0.18f }; // 未展開時の連動橋色
constexpr Math::Vector4 kPlayerPrototypeGoalBridgeDeployedColor = { 1.0f, 0.9f, 0.12f, 1.0f }; // 展開時の連動橋色
constexpr Math::Vector3 kPlayerPrototypeOneWayGateScale = { 0.24f, 1.8f, 2.8f }; // 一方通行ゲートの表示サイズ
constexpr Math::Vector3 kPlayerPrototypeOneWayGateTranslate = { 13.2f, 3.55f, 0.0f }; // 終盤で戻りを塞ぐ一方通行ゲートの中心座標
constexpr Math::Vector4 kPlayerPrototypeOneWayGatePassableColor = { 0.5f, 0.1f, 0.85f, 0.45f }; // 通行可能時の一方通行ゲート色
constexpr Math::Vector4 kPlayerPrototypeOneWayGateBlockingColor = { 1.0f, 0.05f, 1.0f, 1.0f }; // 戻りを塞ぐ時の一方通行ゲート色

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
/// プレイヤー位置に応じて次に画面へ収めるギミック位置を取得する。
/// </summary>
Math::Vector3 GetPlayerPrototypeCameraTarget(const PlayerState& playerState)
{
    const float playerX = playerState.transform.translate.x; // 次のギミックを選ぶプレイヤーX座標
    if (playerX < kPlayerPrototypeDoorTranslate.x) {
        return kPlayerPrototypeDoorTranslate;
    }
    if (playerX < kPlayerPrototypeTimedDoorTranslate.x) {
        return kPlayerPrototypeTimedDoorTranslate;
    }
    if (playerX < kPlayerPrototypeWeightSwitchTranslate.x) {
        return kPlayerPrototypeWeightSwitchTranslate;
    }
    if (playerX < kPlayerPrototypeOneWayGateTranslate.x) {
        return kPlayerPrototypeOneWayGateTranslate;
    }
    return kPlayerPrototypeGoalCenter;
}

/// <summary>
/// プレイヤー、可視分身、次のギミックを収めるカメラ範囲を計算する。
/// </summary>
PlayerPrototypeCameraFrame CalculatePlayerPrototypeCameraFrame(const PlayerState& playerState, const std::vector<PlayerState>& cloneStates)
{
    const Math::Vector3 targetPosition = GetPlayerPrototypeCameraTarget(playerState); // 画面内に含める次のギミック位置
    float minimumX = playerState.transform.translate.x; // 画面内に収める対象の最小X座標
    float maximumX = playerState.transform.translate.x; // 画面内に収める対象の最大X座標
    float minimumY = playerState.transform.translate.y; // 画面内に収める対象の最小Y座標
    float maximumY = playerState.transform.translate.y; // 画面内に収める対象の最大Y座標
    for (const PlayerState& cloneState : cloneStates) {
        minimumX = (std::min)(minimumX, cloneState.transform.translate.x);
        maximumX = (std::max)(maximumX, cloneState.transform.translate.x);
        minimumY = (std::min)(minimumY, cloneState.transform.translate.y);
        maximumY = (std::max)(maximumY, cloneState.transform.translate.y);
    }
    minimumX = (std::min)(minimumX, targetPosition.x);
    maximumX = (std::max)(maximumX, targetPosition.x);
    minimumY = (std::min)(minimumY, targetPosition.y);
    maximumY = (std::max)(maximumY, targetPosition.y);

    Math::Vector3 focus = playerState.transform.translate; // カメラ中心にする基準座標
    focus.x = (minimumX + maximumX) * 0.5f;
    focus.y = (minimumY + maximumY) * 0.5f;
    focus.x += kPlayerPrototypeCameraFocusOffset.x;
    focus.y += kPlayerPrototypeCameraFocusOffset.y;
    focus.z = kPlayerPrototypeCameraFocusOffset.z;

    const float halfFovTangent = std::tan(kPlayerPrototypeCameraFovY * 0.5f); // 縦方向の表示範囲計算に使う視野角係数
    const float horizontalHalfRange = (maximumX - minimumX) * 0.5f + kPlayerPrototypeCameraHorizontalPadding; // 左右余白を含む半幅
    const float verticalHalfRange = (maximumY - minimumY) * 0.5f + kPlayerPrototypeCameraVerticalPadding; // 上下余白を含む半高
    const float horizontalDistance = horizontalHalfRange / (halfFovTangent * kPlayerPrototypeCameraVisibleAspect); // 横幅を収めるための距離
    const float verticalDistance = verticalHalfRange / halfFovTangent; // 高さを収めるための距離
    const float distance = std::clamp((std::max)(horizontalDistance, verticalDistance),
        kPlayerPrototypeCameraMinimumDistance, kPlayerPrototypeCameraMaximumDistance); // 使用範囲に制限したカメラ距離
    return { focus, distance };
}

/// <summary>
/// 現在値を目標値へフレーム時間に応じて追従させる。
/// </summary>
float FollowPlayerPrototypeCameraValue(float currentValue, float targetValue, float deltaTime)
{
    const float followRate = 1.0f - std::exp(-kPlayerPrototypeCameraFollowSpeed * (std::max)(deltaTime, 0.0f)); // フレームレートに依存しにくい追従率
    return currentValue + (targetValue - currentValue) * followRate;
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
void ConfigurePlayerPrototypeCamera(Camera* camera, const Math::Vector3& focus, float distance)
{
    if (!camera) {
        return;
    }

    const Math::Vector3 rotatedFocus = RotatePointForPlayerPrototypeCamera(focus); // ビュー回転後の注視点座標
    const Math::Vector3 cameraTranslate = {
        rotatedFocus.x,
        rotatedFocus.y,
        rotatedFocus.z - distance
    }; // 横視点で注視点を画面中央に置くカメラ位置
    camera->SetTranslate(cameraTranslate);
    camera->SetRotate(kPlayerPrototypeCameraRotate);
    camera->SetFovY(kPlayerPrototypeCameraFovY);
    camera->Update();
}

/// <summary>
/// プレイヤーが上面に乗れる足場一覧を作成する。
/// </summary>
std::vector<StandablePlatform> BuildPlayerStandablePlatforms(const PastSelfCloneManager& cloneManager)
{
    return cloneManager.GetStandablePlatforms();
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

/// <summary>
/// プレイヤーがいずれかの分身足場に立っているか判定する。
/// </summary>
bool IsPlayerStandingOnClonePlatform(const PlayerState& playerState, const std::vector<StandablePlatform>& clonePlatforms)
{
    return std::any_of(clonePlatforms.begin(), clonePlatforms.end(), [&playerState](const StandablePlatform& clonePlatform) {
        return IsPlayerStateStandingOnPlatform(playerState, clonePlatform);
    });
}
} // namespace

/// <summary>
/// プレイヤー確認用オブジェクトを初期化する。
/// </summary>
void PlayScene::InitializePlayerPrototype()
{
    InitializePlayerPrototypeStage();
    InitializePlayerPrototypeMechanics();
    pastSelfRecorder_.SetMaxRecordTime(playerPrototypeStageRules_.maxRecordTime);
    player_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, kPlayerPrototypeModelFileName);
    player_.SetMaterialColor(kPlayerPrototypeNormalPlayerColor);
    PlayerState playerStartState = player_.GetState(); // 仮ステージに合わせた開始状態
    playerStartState.transform.translate = kPlayerPrototypeStartTranslate;
    player_.SetInitialState(playerStartState);
    pastSelfCloneManager_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, kPlayerPrototypeModelFileName);
    const PlayerPrototypeCameraFrame initialCameraFrame = CalculatePlayerPrototypeCameraFrame(player_.GetState(), {}); // 初期位置と最初のギミックを収めるカメラ範囲
    playerPrototypeCameraFocus_ = initialCameraFrame.focus;
    playerPrototypeCameraDistance_ = initialCameraFrame.distance;
    ConfigurePlayerPrototypeCamera(ctx_.camera, playerPrototypeCameraFocus_, playerPrototypeCameraDistance_);
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

    ToggleSwitchGimmickDesc toggleSwitchDesc {}; // 任意試作区間のトグルスイッチ初期化情報
    toggleSwitchDesc.objectId = IssueObjectId();
    toggleSwitchDesc.modelFileName = kPlayerPrototypeModelFileName;
    toggleSwitchDesc.scale = kPlayerPrototypeToggleSwitchScale;
    toggleSwitchDesc.translate = kPlayerPrototypeToggleSwitchTranslate;
    toggleSwitchDesc.volumeCenter = kPlayerPrototypeToggleSwitchVolumeCenter;
    toggleSwitchDesc.volumeHalfSize = kPlayerPrototypeToggleSwitchVolumeHalfSize;
    toggleSwitchDesc.inactiveColor = kPlayerPrototypeToggleSwitchInactiveColor;
    toggleSwitchDesc.activeColor = kPlayerPrototypeToggleSwitchActiveColor;
    toggleSwitchDesc.pressedColor = kPlayerPrototypeToggleSwitchPressedColor;

    LinkedDoorGimmickDesc toggleGateDesc {}; // トグルスイッチに連動する任意試作区間ゲートの初期化情報
    toggleGateDesc.objectId = IssueObjectId();
    toggleGateDesc.modelFileName = kPlayerPrototypeModelFileName;
    toggleGateDesc.scale = kPlayerPrototypeToggleGateScale;
    toggleGateDesc.translate = kPlayerPrototypeToggleGateTranslate;
    toggleGateDesc.closedColor = kPlayerPrototypeToggleGateClosedColor;
    toggleGateDesc.openColor = kPlayerPrototypeToggleGateOpenColor;

    MovingPlatformGimmickDesc toggleElevatorDesc {}; // トグルスイッチに連動する昇降足場の初期化情報
    toggleElevatorDesc.objectId = IssueObjectId();
    toggleElevatorDesc.modelFileName = kPlayerPrototypeModelFileName;
    toggleElevatorDesc.scale = kPlayerPrototypeToggleElevatorScale;
    toggleElevatorDesc.lowerTranslate = kPlayerPrototypeToggleElevatorLowerTranslate;
    toggleElevatorDesc.upperTranslate = kPlayerPrototypeToggleElevatorUpperTranslate;
    toggleElevatorDesc.inactiveColor = kPlayerPrototypeToggleElevatorInactiveColor;
    toggleElevatorDesc.activeColor = kPlayerPrototypeToggleElevatorActiveColor;
    toggleElevatorDesc.moveSpeed = kPlayerPrototypeToggleElevatorMoveSpeed;
    toggleElevatorDesc.upperWaitSeconds = kPlayerPrototypeToggleElevatorUpperWaitSeconds;
    toggleElevatorDesc.lowerWaitSeconds = kPlayerPrototypeToggleElevatorLowerWaitSeconds;

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

    LinkedBridgeGimmickDesc goalBridgeDesc {}; // 重さスイッチに連動するゴール前の橋の初期化情報
    goalBridgeDesc.objectId = IssueObjectId();
    goalBridgeDesc.modelFileName = kPlayerPrototypeModelFileName;
    goalBridgeDesc.scale = kPlayerPrototypeGoalBridgeScale;
    goalBridgeDesc.translate = kPlayerPrototypeGoalBridgeTranslate;
    goalBridgeDesc.retractedColor = kPlayerPrototypeGoalBridgeRetractedColor;
    goalBridgeDesc.deployedColor = kPlayerPrototypeGoalBridgeDeployedColor;

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
    playerPrototypeToggleSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, toggleSwitchDesc);
    playerPrototypeToggleGate_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, toggleGateDesc);
    playerPrototypeToggleElevator_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, toggleElevatorDesc);
    playerPrototypeWeightSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, weightSwitchDesc);
    playerPrototypeGoalBridge_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, goalBridgeDesc);
    playerPrototypeOneWayGate_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, oneWayGateDesc);
    playerPrototypeGoal_.Configure(goalDesc);
    playerPrototypeCloneStartMarkerObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeCloneStartMarkerScale, kPlayerPrototypeStartTranslate, kPlayerPrototypeCloneStartMarkerColor);
    playerPrototypeCloneEndMarkerObject_ = CreatePlayerPrototypeBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPlayerPrototypeCloneEndMarkerScale, kPlayerPrototypeStartTranslate, kPlayerPrototypeCloneEndMarkerColor);
    playerPrototypeSwitchActive_ = false;
    playerPrototypeDoorOpen_ = false;
    playerPrototypeDoorUnlockedByClone_ = false;
    playerPrototypePlayerOnSwitch_ = false;
    playerPrototypeCloneOnSwitch_ = false;
    playerPrototypeDoorBlockedBeforeClone_ = false;
    playerPrototypeClonePlatformUsed_ = false;
    playerPrototypeDoorOpenedByClone_ = false;
    playerPrototypeTimedSwitchActive_ = false;
    playerPrototypeTimedSwitchCloneOn_ = false;
    playerPrototypeTimedDoorOpen_ = false;
    playerPrototypeToggleSwitchActive_ = false;
    playerPrototypeToggleSwitchCloneOn_ = false;
    playerPrototypeToggleGateOpen_ = false;
    playerPrototypeToggleElevatorActive_ = false;
    playerPrototypeOneCloneToggleActivated_ = false;
    playerPrototypeOneCloneElevatorRidden_ = false;
    playerPrototypeOneCloneTutorialComplete_ = false;
    playerPrototypeTwoCloneReplayPrepared_ = false;
    playerPrototypeTwoCloneSwitchesActivated_ = false;
    playerPrototypeTwoCloneTutorialComplete_ = false;
    playerPrototypeWeightSwitchActive_ = false;
    playerPrototypeWeightPlayerOn_ = false;
    playerPrototypeWeightCloneOn_ = false;
    playerPrototypeGoalBridgeUnlocked_ = false;
    playerPrototypeGoalBridgeDeployed_ = false;
    playerPrototypeOneWayGateBlocking_ = false;
    playerPrototypeTimedDoorOpened_ = false;
    playerPrototypeDualCloneSwitchesActivated_ = false;
    playerPrototypeWeightSwitchActivated_ = false;
    playerPrototypeOneWayGateUsed_ = false;
    playerPrototypeResetShown_ = true;
    playerPrototypeRecordStarted_ = false;
    playerPrototypeRecordStopped_ = false;
    playerPrototypePrepareUsed_ = false;
    playerPrototypeReplayStarted_ = false;
    playerPrototypeRecordingPendingCommit_ = false;
    playerPrototypeElapsedTime_ = 0.0f;
    playerPrototypeClearTime_ = 0.0f;
    playerPrototypeLastRecordDuration_ = 0.0f;
    playerPrototypePrepareFeedbackSeconds_ = 0.0f;
    playerPrototypeRecentCheckText_.clear();
    playerPrototypeRecentCheckSeconds_ = 0.0f;
    playerPrototypeRecordTakeCount_ = 0;
}

/// <summary>
/// プレイヤー確認用の分身ギミックを更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototypeMechanics(float deltaTime)
{
    const PlayerState& playerState = player_.GetState(); // ギミック判定に使う現在のプレイヤー状態
    const std::vector<PlayerState> cloneStates = pastSelfCloneManager_.GetVisibleStates(); // ギミック入力に使用する可視分身状態一覧
    const std::span<const PlayerState> cloneStateView { cloneStates }; // ギミックへ渡す分身状態の参照範囲
    const bool playbackCloneVisible = !cloneStates.empty(); // 再生中または表示中の分身があるか
    std::vector<PlayerState> timedSwitchInputStates = cloneStates; // 時間差スイッチへ入力する分身と記録中プレイヤーの状態一覧
    if (pastSelfRecorder_.IsRecording()) {
        timedSwitchInputStates.push_back(playerState);
    }
    const std::span<const PlayerState> timedSwitchInputView { timedSwitchInputStates }; // 時間差スイッチへ渡す状態の参照範囲
    playerPrototypeSwitch_.Update(playerState, cloneStateView);
    playerPrototypePlayerOnSwitch_ = playerPrototypeSwitch_.IsPlayerOnSwitch();
    playerPrototypeCloneOnSwitch_ = playerPrototypeSwitch_.IsCloneOnSwitch();
    playerPrototypeSwitchActive_ = playerPrototypeSwitch_.IsActive();
    if (playbackCloneVisible && playerPrototypeSwitchActive_) {
        playerPrototypeDoorUnlockedByClone_ = true;
    }
    playerPrototypeDoor_.Update(playerPrototypeDoorUnlockedByClone_);
    playerPrototypeDoorOpen_ = playerPrototypeDoor_.IsOpen();

    playerPrototypeTimedSwitch_.Update(deltaTime, timedSwitchInputView);
    playerPrototypeTimedSwitchActive_ = playerPrototypeTimedSwitch_.IsActive();
    playerPrototypeTimedSwitchCloneOn_ = playerPrototypeTimedSwitch_.IsCloneOnSwitch();
    playerPrototypeWeightSwitch_.Update(playerState, cloneStateView);
    playerPrototypeWeightSwitchActive_ = playerPrototypeWeightSwitch_.IsActive();
    playerPrototypeWeightPlayerOn_ = playerPrototypeWeightSwitch_.IsPlayerOnSwitch();
    playerPrototypeWeightCloneOn_ = playerPrototypeWeightSwitch_.IsCloneOnSwitch();
    const bool dualCloneSwitchInputActive = playerPrototypeSwitchActive_ && playerPrototypeTimedSwitchActive_; // 緑と青のスイッチが同時に起動しているか
    playerPrototypeTimedDoor_.Update(dualCloneSwitchInputActive || playerPrototypeWeightSwitchActive_);
    playerPrototypeTimedDoorOpen_ = playerPrototypeTimedDoor_.IsOpen();
    playerPrototypeToggleSwitch_.Update(cloneStateView);
    playerPrototypeToggleSwitchActive_ = playerPrototypeToggleSwitch_.IsActive();
    playerPrototypeToggleSwitchCloneOn_ = playerPrototypeToggleSwitch_.IsCloneOnSwitch();
    playerPrototypeToggleGate_.Update(playerPrototypeToggleSwitchActive_);
    playerPrototypeToggleGateOpen_ = playerPrototypeToggleGate_.IsOpen();
    playerPrototypeToggleElevator_.Update(deltaTime, playerPrototypeToggleSwitchActive_);
    playerPrototypeToggleElevatorActive_ = playerPrototypeToggleElevator_.IsActive();
    const bool oneCloneTutorialEligible = playerPrototypeRecordTakeCount_ == 1 &&
        pastSelfCloneManager_.GetCloneCount() == 1 && playerPrototypeRecordStopped_ &&
        playerPrototypePrepareUsed_ && playerPrototypeReplayStarted_; // 1回の記録と1体の分身で再生準備まで行ったか
    if (oneCloneTutorialEligible && playerPrototypeToggleSwitchCloneOn_ && playerPrototypeToggleSwitchActive_ &&
        !playerPrototypeOneCloneToggleActivated_) {
        playerPrototypeOneCloneToggleActivated_ = true;
        RegisterPlayerPrototypeCheckCompleted("Clone activated the orange toggle");
    }
    playerPrototypeOneWayGate_.Update(player_.GetState());
    playerPrototypeOneWayGateBlocking_ = playerPrototypeOneWayGate_.IsBlocking();

    if (playbackCloneVisible && playerPrototypeCloneOnSwitch_ && !playerPrototypeDoorOpenedByClone_) {
        playerPrototypeDoorOpenedByClone_ = true;
        RegisterPlayerPrototypeCheckCompleted("Clone opened green door");
    }
    if (playerPrototypeTimedDoorOpen_ && dualCloneSwitchInputActive && !playerPrototypeTimedDoorOpened_) {
        playerPrototypeTimedDoorOpened_ = true;
        RegisterPlayerPrototypeCheckCompleted("Green and blue switches opened blue door");
    }
    if (!pastSelfRecorder_.IsRecording() && cloneStates.size() >= 2 && playerPrototypeSwitchActive_ && playerPrototypeTimedSwitchCloneOn_) {
        playerPrototypeDualCloneSwitchesActivated_ = true;
    }
    const bool twoCloneTutorialEligible = !pastSelfRecorder_.IsRecording() &&
        playerPrototypeRecordTakeCount_ == 2 && pastSelfCloneManager_.GetCloneCount() == 2 &&
        playerPrototypeRecordStopped_ && playerPrototypeTwoCloneReplayPrepared_; // 2回の記録と2体の分身で再生準備したか
    if (twoCloneTutorialEligible && cloneStates.size() == 2 && playerPrototypeSwitchActive_ &&
        playerPrototypeTimedSwitchCloneOn_ && !playerPrototypeTwoCloneSwitchesActivated_) {
        playerPrototypeTwoCloneSwitchesActivated_ = true;
        RegisterPlayerPrototypeCheckCompleted("Two clones activated green and blue switches");
    }
    if (playerPrototypeWeightSwitchActive_ && !playerPrototypeWeightSwitchActivated_) {
        playerPrototypeWeightSwitchActivated_ = true;
        RegisterPlayerPrototypeCheckCompleted("Player and clone activated yellow switch");
    }
    if (playerPrototypeWeightSwitchActive_) {
        playerPrototypeGoalBridgeUnlocked_ = true;
    }
    playerPrototypeGoalBridge_.Update(playerPrototypeGoalBridgeUnlocked_);
    playerPrototypeGoalBridgeDeployed_ = playerPrototypeGoalBridge_.IsDeployed();
    if (playerPrototypeOneWayGateBlocking_ && !playerPrototypeOneWayGateUsed_) {
        playerPrototypeOneWayGateUsed_ = true;
        RegisterPlayerPrototypeCheckCompleted("Purple gate blocked the return path");
    }
}

/// <summary>
/// プレイヤー確認用状態を初期状態へ戻す。
/// </summary>
void PlayScene::ResetPlayerPrototypeState()
{
    player_.Reset();
    pastSelfRecorder_.Clear();
    pastSelfCloneManager_.Clear();
    playerPrototypeGoal_.Reset();
    playerPrototypeSwitch_.Reset();
    playerPrototypeDoor_.Reset();
    playerPrototypeTimedSwitch_.Reset();
    playerPrototypeTimedDoor_.Reset();
    playerPrototypeToggleSwitch_.Reset();
    playerPrototypeToggleGate_.Reset();
    playerPrototypeToggleElevator_.Reset();
    playerPrototypeWeightSwitch_.Reset();
    playerPrototypeGoalBridge_.Reset();
    playerPrototypeOneWayGate_.Reset();
    playerPrototypeGoalReached_ = false;
    playerPrototypeSwitchActive_ = false;
    playerPrototypeDoorOpen_ = false;
    playerPrototypeDoorUnlockedByClone_ = false;
    playerPrototypePlayerOnSwitch_ = false;
    playerPrototypeCloneOnSwitch_ = false;
    playerPrototypeDoorBlockedBeforeClone_ = false;
    playerPrototypeClonePlatformUsed_ = false;
    playerPrototypeDoorOpenedByClone_ = false;
    playerPrototypeTimedSwitchActive_ = false;
    playerPrototypeTimedSwitchCloneOn_ = false;
    playerPrototypeTimedDoorOpen_ = false;
    playerPrototypeToggleSwitchActive_ = false;
    playerPrototypeToggleSwitchCloneOn_ = false;
    playerPrototypeToggleGateOpen_ = false;
    playerPrototypeToggleElevatorActive_ = false;
    playerPrototypeOneCloneToggleActivated_ = false;
    playerPrototypeOneCloneElevatorRidden_ = false;
    playerPrototypeOneCloneTutorialComplete_ = false;
    playerPrototypeTwoCloneReplayPrepared_ = false;
    playerPrototypeTwoCloneSwitchesActivated_ = false;
    playerPrototypeTwoCloneTutorialComplete_ = false;
    playerPrototypeWeightSwitchActive_ = false;
    playerPrototypeWeightPlayerOn_ = false;
    playerPrototypeWeightCloneOn_ = false;
    playerPrototypeGoalBridgeUnlocked_ = false;
    playerPrototypeGoalBridgeDeployed_ = false;
    playerPrototypeOneWayGateBlocking_ = false;
    playerPrototypeTimedDoorOpened_ = false;
    playerPrototypeDualCloneSwitchesActivated_ = false;
    playerPrototypeWeightSwitchActivated_ = false;
    playerPrototypeOneWayGateUsed_ = false;
    playerPrototypeResetShown_ = true;
    playerPrototypeRecordStarted_ = false;
    playerPrototypeRecordStopped_ = false;
    playerPrototypePrepareUsed_ = false;
    playerPrototypeReplayStarted_ = false;
    playerPrototypeRecordingPendingCommit_ = false;
    playerPrototypeElapsedTime_ = 0.0f;
    playerPrototypeClearTime_ = 0.0f;
    playerPrototypeLastRecordDuration_ = 0.0f;
    playerPrototypePrepareFeedbackSeconds_ = 0.0f;
    playerPrototypeRecentCheckText_.clear();
    playerPrototypeRecentCheckSeconds_ = 0.0f;
    playerPrototypeRecordTakeCount_ = 0;
    player_.SetMaterialColor(kPlayerPrototypeNormalPlayerColor);
    const PlayerPrototypeCameraFrame resetCameraFrame = CalculatePlayerPrototypeCameraFrame(player_.GetState(), {}); // リセット直後の開始地点と最初のギミックを収める範囲
    playerPrototypeCameraFocus_ = resetCameraFrame.focus;
    playerPrototypeCameraDistance_ = resetCameraFrame.distance;
    UpdatePlayerPrototypeMechanics(0.0f);
    ApplyPlayerPrototypeGoalVisual();
}

/// <summary>
/// 記録済み分身を残したまま再生開始用の状態へ戻す。
/// </summary>
void PlayScene::ResetPlayerPrototypeReplayState(bool registerPrepareAction)
{
    const bool keepDoorBlocked = playerPrototypeDoorBlockedBeforeClone_; // 記録前に閉じた扉へ阻まれた実証結果
    const bool keepClonePlatformUsed = playerPrototypeClonePlatformUsed_; // 分身足場を利用した実証結果
    const bool keepDoorOpenedByClone = playerPrototypeDoorOpenedByClone_; // 分身で通常扉を開けた実証結果
    const bool keepTimedDoorOpened = playerPrototypeTimedDoorOpened_; // 時間差扉を開けた実証結果
    const bool keepDualCloneSwitchesActivated = playerPrototypeDualCloneSwitchesActivated_; // 複数分身で離れたスイッチを同時起動した実証結果
    const bool keepWeightSwitchActivated = playerPrototypeWeightSwitchActivated_; // 重さスイッチを起動した実証結果
    const bool keepGoalBridgeUnlocked = playerPrototypeGoalBridgeUnlocked_; // 重さスイッチで解放した橋の攻略状態
    const bool keepOneWayGateUsed = playerPrototypeOneWayGateUsed_; // 一方通行ゲートを利用した実証結果
    const bool keepResetShown = playerPrototypeResetShown_; // リセット開始を示す実証結果
    const bool keepRecordStarted = playerPrototypeRecordStarted_; // 記録開始を示す実証結果
    const bool keepRecordStopped = playerPrototypeRecordStopped_; // 記録停止を示す実証結果
    const bool keepPrepareUsed = playerPrototypePrepareUsed_; // Prepare操作を示す実証結果
    const bool keepReplayStarted = playerPrototypeReplayStarted_; // 再生開始を示す実証結果
    const bool keepDoorUnlocked = keepDoorOpenedByClone; // 再生済み分身で開放した通常扉状態
    player_.Reset();
    pastSelfCloneManager_.StopAll();
    playerPrototypeGoal_.Reset();
    playerPrototypeSwitch_.Reset();
    playerPrototypeDoor_.Reset();
    playerPrototypeTimedSwitch_.Reset();
    playerPrototypeTimedDoor_.Reset();
    playerPrototypeToggleSwitch_.Reset();
    playerPrototypeToggleGate_.Reset();
    playerPrototypeToggleElevator_.Reset();
    playerPrototypeWeightSwitch_.Reset();
    playerPrototypeGoalBridge_.Reset();
    playerPrototypeOneWayGate_.Reset();
    playerPrototypeGoalReached_ = false;
    playerPrototypeSwitchActive_ = false;
    playerPrototypeDoorOpen_ = keepDoorUnlocked;
    playerPrototypeDoorUnlockedByClone_ = keepDoorUnlocked;
    playerPrototypePlayerOnSwitch_ = false;
    playerPrototypeCloneOnSwitch_ = false;
    playerPrototypeDoorBlockedBeforeClone_ = keepDoorBlocked;
    playerPrototypeClonePlatformUsed_ = keepClonePlatformUsed;
    playerPrototypeDoorOpenedByClone_ = keepDoorOpenedByClone;
    playerPrototypeTimedSwitchActive_ = false;
    playerPrototypeTimedSwitchCloneOn_ = false;
    playerPrototypeTimedDoorOpen_ = false;
    playerPrototypeToggleSwitchActive_ = false;
    playerPrototypeToggleSwitchCloneOn_ = false;
    playerPrototypeToggleGateOpen_ = false;
    playerPrototypeToggleElevatorActive_ = false;
    playerPrototypeOneCloneToggleActivated_ = false;
    playerPrototypeOneCloneElevatorRidden_ = false;
    playerPrototypeOneCloneTutorialComplete_ = false;
    playerPrototypeTwoCloneReplayPrepared_ = registerPrepareAction &&
        playerPrototypeRecordTakeCount_ == 2 && pastSelfCloneManager_.GetCloneCount() == 2;
    playerPrototypeTwoCloneSwitchesActivated_ = false;
    playerPrototypeTwoCloneTutorialComplete_ = false;
    playerPrototypeWeightSwitchActive_ = false;
    playerPrototypeWeightPlayerOn_ = false;
    playerPrototypeWeightCloneOn_ = false;
    playerPrototypeGoalBridgeUnlocked_ = keepGoalBridgeUnlocked;
    playerPrototypeGoalBridgeDeployed_ = keepGoalBridgeUnlocked;
    playerPrototypeOneWayGateBlocking_ = false;
    playerPrototypeTimedDoorOpened_ = keepTimedDoorOpened;
    playerPrototypeDualCloneSwitchesActivated_ = keepDualCloneSwitchesActivated;
    playerPrototypeWeightSwitchActivated_ = keepWeightSwitchActivated;
    playerPrototypeOneWayGateUsed_ = keepOneWayGateUsed;
    playerPrototypeResetShown_ = keepResetShown;
    playerPrototypeRecordStarted_ = keepRecordStarted;
    playerPrototypeRecordStopped_ = keepRecordStopped;
    playerPrototypePrepareUsed_ = keepPrepareUsed || registerPrepareAction;
    playerPrototypeReplayStarted_ = keepReplayStarted;
    playerPrototypeRecordingPendingCommit_ = false;
    playerPrototypePrepareFeedbackSeconds_ = registerPrepareAction ? kPrepareFeedbackDuration : 0.0f;
    playerPrototypeClearTime_ = 0.0f;
    player_.SetMaterialColor(kPlayerPrototypeNormalPlayerColor);
    const PlayerPrototypeCameraFrame replayCameraFrame = CalculatePlayerPrototypeCameraFrame(player_.GetState(), {}); // Prepare直後の開始地点と最初のギミックを収める範囲
    playerPrototypeCameraFocus_ = replayCameraFrame.focus;
    playerPrototypeCameraDistance_ = replayCameraFrame.distance;
    UpdatePlayerPrototypeMechanics(0.0f);
    ApplyPlayerPrototypeGoalVisual();
}

/// <summary>
/// 最後に保存した分身を削除して再生準備状態へ戻す。
/// </summary>
void PlayScene::UndoLastPlayerPrototypeClone()
{
    if (pastSelfRecorder_.IsRecording() || !pastSelfCloneManager_.RemoveLastClone()) {
        return;
    }

    playerPrototypeLastRecordDuration_ = pastSelfCloneManager_.GetLastCloneDuration();
    ResetPlayerPrototypeReplayState(false);
}

/// <summary>
/// 現在のステージルールで新しい分身記録を開始できるか判定する。
/// </summary>
bool PlayScene::CanStartPlayerPrototypeRecording() const
{
    return !playerPrototypeGoalReached_ && !pastSelfRecorder_.IsRecording() &&
        pastSelfCloneManager_.GetCloneCount() < playerPrototypeStageRules_.maxStoredClones;
}

/// <summary>
/// 新しい分身用のプレイヤー記録を開始する。
/// </summary>
void PlayScene::StartPlayerPrototypeRecording()
{
    if (!CanStartPlayerPrototypeRecording()) {
        return;
    }

    if (pastSelfCloneManager_.StartAll()) {
        playerPrototypeReplayStarted_ = true;
    }
    pastSelfRecorder_.Start();
    ++playerPrototypeRecordTakeCount_;
    playerPrototypeRecordStarted_ = true;
    playerPrototypeRecordingPendingCommit_ = true;
    playerPrototypeLastRecordDuration_ = 0.0f;
}

/// <summary>
/// 現在の記録を停止し、新しい分身として保存する。
/// </summary>
void PlayScene::FinishPlayerPrototypeRecording()
{
    if (!playerPrototypeRecordingPendingCommit_) {
        return;
    }

    pastSelfRecorder_.Stop();
    playerPrototypeLastRecordDuration_ = pastSelfRecorder_.GetDuration();
    const bool cloneAdded = pastSelfCloneManager_.AddClone(pastSelfRecorder_.GetFrames()); // 有効な記録から分身を保存できたか
    playerPrototypeRecordStopped_ = playerPrototypeRecordStopped_ || cloneAdded;
    playerPrototypeRecordingPendingCommit_ = false;
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
    playerPrototypeToggleSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeToggleGate_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeToggleElevator_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeWeightSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    playerPrototypeGoalBridge_.UpdateObject(viewMatrix, projectionMatrix);
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
    playerPrototypeToggleSwitch_.Draw();
    playerPrototypeToggleGate_.Draw();
    playerPrototypeToggleElevator_.Draw();
    playerPrototypeWeightSwitch_.Draw();
    playerPrototypeGoalBridge_.Draw();
    playerPrototypeOneWayGate_.Draw();
}

/// <summary>
/// プレイヤー確認用状態を更新する。
/// </summary>
void PlayScene::UpdatePlayerPrototype(float deltaTime)
{
    playerPrototypePrepareFeedbackSeconds_ = (std::max)(playerPrototypePrepareFeedbackSeconds_ - deltaTime, 0.0f);
    playerPrototypeRecentCheckSeconds_ = (std::max)(playerPrototypeRecentCheckSeconds_ - deltaTime, 0.0f);
    const bool blockInputByImGui = ShouldBlockPlayerInput(); // ImGui操作でゲーム入力を止めるか
    InputManager* inputManager = InputManager::GetInstance(); // プレイヤー確認用入力を取得する管理クラス
    if (!blockInputByImGui && inputManager && inputManager->IsKeyJustPressed(kPrototypeResetKey)) {
        ResetPlayerPrototypeState();
    }
    const bool canPrepareReplay = !playerPrototypeGoalReached_ && !blockInputByImGui && inputManager &&
        !pastSelfRecorder_.IsRecording() && pastSelfCloneManager_.GetCloneCount() > 0; // Prepare入力を受け付けられるか
    if (canPrepareReplay && inputManager->IsKeyJustPressed(kReplayPrepareKey)) {
        ResetPlayerPrototypeReplayState();
    }
    const bool canUndoLastClone = !playerPrototypeGoalReached_ && !blockInputByImGui && inputManager &&
        !pastSelfRecorder_.IsRecording() && pastSelfCloneManager_.GetCloneCount() > 0; // 分身削除入力を受け付けられるか
    if (canUndoLastClone && inputManager->IsKeyJustPressed(kCloneUndoKey)) {
        UndoLastPlayerPrototypeClone();
    }

    const bool canAcceptInput = !playerPrototypeGoalReached_ && !blockInputByImGui; // プレイヤーと分身操作の入力を受け取れるか
    if (canAcceptInput && inputManager) {
        if (inputManager->IsKeyJustPressed(kRecordToggleKey)) {
            if (pastSelfRecorder_.IsRecording()) {
                FinishPlayerPrototypeRecording();
            } else {
                StartPlayerPrototypeRecording();
            }
        }
        if (inputManager->IsKeyJustPressed(kClonePlayKey)) {
            if (pastSelfCloneManager_.StartAll()) {
                playerPrototypeReplayStarted_ = true;
            }
        }
        if (inputManager->IsKeyJustPressed(kCloneStopKey)) {
            pastSelfCloneManager_.StopAll();
        }
    }

    if (!playerPrototypeGoalReached_) {
        playerPrototypeElapsedTime_ += deltaTime;
        pastSelfCloneManager_.Update(deltaTime, BuildCloneStandablePlatforms(player_));
        UpdatePlayerPrototypeMechanics(deltaTime);
        const SolidCollider previousElevatorCollider = playerPrototypeToggleElevator_.GetPreviousSolidCollider(); // 更新前の昇降足場コライダー
        const SolidCollider currentElevatorCollider = playerPrototypeToggleElevator_.GetSolidCollider(); // 更新後の昇降足場コライダー
        std::vector<SolidCollider> solidColliders; // プレイヤーが全面衝突する地形と扉
        AppendPlayerPrototypeSolidColliders(&solidColliders);
        player_.ResolveMovingSolidCollider(previousElevatorCollider, currentElevatorCollider);
        player_.ResolveExternalSolidCollisions(solidColliders, currentElevatorCollider);
        std::vector<StandablePlatform> standablePlatforms = BuildPlayerStandablePlatforms(pastSelfCloneManager_); // プレイヤーが上面だけ乗れる分身足場
        player_.Update(deltaTime, canAcceptInput, solidColliders, standablePlatforms);
        const StandablePlatform toggleElevatorPlatform {
            currentElevatorCollider.center,
            currentElevatorCollider.halfSize,
            currentElevatorCollider.enabled
        }; // 昇降足場の上面利用を判定する足場情報
        const bool playerStandingOnToggleElevator = IsPlayerStateStandingOnPlatform(player_.GetState(), toggleElevatorPlatform); // プレイヤーが昇降足場上に立っているか
        if (playerPrototypeOneCloneToggleActivated_ && playerStandingOnToggleElevator &&
            !playerPrototypeOneCloneElevatorRidden_) {
            playerPrototypeOneCloneElevatorRidden_ = true;
            RegisterPlayerPrototypeCheckCompleted("Player rode the orange moving lift");
        }
        constexpr float kElevatorEndpointTolerance = 0.05f; // 上端到達判定に許容する位置誤差
        const bool elevatorAtUpperEndpoint = std::fabs(
            playerPrototypeToggleElevator_.GetCurrentTranslate().y - kPlayerPrototypeToggleElevatorUpperTranslate.y) <=
            kElevatorEndpointTolerance; // 昇降足場が上端へ到達しているか
        if (playerPrototypeOneCloneElevatorRidden_ && playerStandingOnToggleElevator && elevatorAtUpperEndpoint &&
            !playerPrototypeOneCloneTutorialComplete_) {
            playerPrototypeOneCloneTutorialComplete_ = true;
            RegisterPlayerPrototypeCheckCompleted("One-clone tutorial route complete");
        }
        const Math::Vector3 playerHalfSize = CalculatePlayerStateHalfSize(player_.GetState()); // 青扉通過判定に使うプレイヤー半サイズ
        const float timedDoorRightEdge = kPlayerPrototypeTimedDoorTranslate.x +
            std::fabs(kPlayerPrototypeTimedDoorScale.x) * 0.5f; // 青扉の右端X座標
        const bool playerPassedTimedDoor = player_.GetState().transform.translate.x - playerHalfSize.x > timedDoorRightEdge; // プレイヤー全体が青扉の右側へ抜けたか
        if (playerPrototypeTwoCloneSwitchesActivated_ && playerPrototypeTimedDoorOpened_ &&
            playerPassedTimedDoor && !playerPrototypeTwoCloneTutorialComplete_) {
            playerPrototypeTwoCloneTutorialComplete_ = true;
            RegisterPlayerPrototypeCheckCompleted("Two-clone tutorial route complete");
        }
        const SolidCollider doorCollider = playerPrototypeDoor_.GetSolidCollider(); // 閉じている扉の衝突判定
        if (doorCollider.enabled && IsPlayerStateTouchingSolidCollider(player_.GetState(), doorCollider) && !playerPrototypeDoorBlockedBeforeClone_) {
            playerPrototypeDoorBlockedBeforeClone_ = true;
            RegisterPlayerPrototypeCheckCompleted("Closed green door blocked the player");
        }
        if (IsPlayerStandingOnClonePlatform(player_.GetState(), standablePlatforms) && !playerPrototypeClonePlatformUsed_) {
            playerPrototypeClonePlatformUsed_ = true;
            RegisterPlayerPrototypeCheckCompleted("Player used a clone as a platform");
        }
        if (player_.GetState().transform.translate.y < kPlayerPrototypeFallResetY) {
            ResetPlayerPrototypeState();
        }
        const bool wasRecording = pastSelfRecorder_.IsRecording(); // 記録更新前に記録中だったか
        pastSelfRecorder_.Update(deltaTime, player_.GetState());
        if (wasRecording && !pastSelfRecorder_.IsRecording()) {
            FinishPlayerPrototypeRecording();
        }
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

    const std::vector<PlayerState> visibleCloneStates = pastSelfCloneManager_.GetVisibleStates(); // カメラ範囲に含める可視分身状態一覧
    const PlayerPrototypeCameraFrame targetCameraFrame = CalculatePlayerPrototypeCameraFrame(player_.GetState(), visibleCloneStates); // 現在の攻略対象を収める目標カメラ範囲
    playerPrototypeCameraFocus_.x = FollowPlayerPrototypeCameraValue(playerPrototypeCameraFocus_.x, targetCameraFrame.focus.x, deltaTime);
    playerPrototypeCameraFocus_.y = FollowPlayerPrototypeCameraValue(playerPrototypeCameraFocus_.y, targetCameraFrame.focus.y, deltaTime);
    playerPrototypeCameraFocus_.z = FollowPlayerPrototypeCameraValue(playerPrototypeCameraFocus_.z, targetCameraFrame.focus.z, deltaTime);
    playerPrototypeCameraDistance_ = FollowPlayerPrototypeCameraValue(playerPrototypeCameraDistance_, targetCameraFrame.distance, deltaTime);
    ConfigurePlayerPrototypeCamera(ctx_.camera, playerPrototypeCameraFocus_, playerPrototypeCameraDistance_);

    if (!ctx_.camera) {
        return;
    }

    const Math::Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // プレイヤー更新に使用するビュー行列
    const Math::Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // プレイヤー更新に使用する射影行列
    UpdatePlayerPrototypeStage(viewMatrix, projectionMatrix);
    UpdatePlayerPrototypeMechanicObjects(viewMatrix, projectionMatrix);
    UpdatePlayerPrototypeCloneRecordMarkers(viewMatrix, projectionMatrix);
    player_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfCloneManager_.UpdateObjects(viewMatrix, projectionMatrix);
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

    const SolidCollider toggleGateCollider = playerPrototypeToggleGate_.GetSolidCollider(); // 閉じているトグル連動ゲートの全面コライダー
    if (toggleGateCollider.enabled) {
        colliders->push_back(toggleGateCollider);
    }

    const SolidCollider toggleElevatorCollider = playerPrototypeToggleElevator_.GetSolidCollider(); // 現在位置のトグル連動昇降足場コライダー
    if (toggleElevatorCollider.enabled) {
        colliders->push_back(toggleElevatorCollider);
    }

    const SolidCollider goalBridgeCollider = playerPrototypeGoalBridge_.GetSolidCollider(); // 展開中のゴール前の橋コライダー
    if (goalBridgeCollider.enabled) {
        colliders->push_back(goalBridgeCollider);
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
    if (playerPrototypeGoalReached_ || pastSelfRecorder_.IsRecording()) {
        return;
    }

    if (!playerPrototypeGoal_.Update(player_.GetState())) {
        return;
    }

    playerPrototypeGoalReached_ = true;
    RegisterPlayerPrototypeCheckCompleted("Goal reached");
    playerPrototypeClearTime_ = playerPrototypeElapsedTime_;
    playerPrototypeLastRecordDuration_ = pastSelfRecorder_.GetDuration();
    pastSelfRecorder_.Stop();
    pastSelfCloneManager_.PauseAll();
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
    pastSelfCloneManager_.Draw();
    player_.Draw();
}

/// <summary>
/// ImGuiでプレイヤー確認用の状態を表示する。
/// </summary>
void PlayScene::DrawPlayerPrototypeImGui()
{
#ifdef USE_IMGUI
    const size_t storedCloneCount = pastSelfCloneManager_.GetCloneCount(); // 現在保存している分身数
    const bool cloneSlotsFull = storedCloneCount >= playerPrototypeStageRules_.maxStoredClones; // 保存枠が上限へ到達したか
    ImGui::Text("Move: A/D or Left Stick X");
    ImGui::Text("Jump: Space or GamePad A");
    ImGui::TextWrapped("C: Record next + replay stored  V: Replay stored only  B: Stop clones");
    ImGui::TextWrapped("T: Prepare replay  X: Undo last clone  R: Reset puzzle");
    if (playerPrototypeShowVerificationDetails_) {
        ImGui::Text("Switch: %s", playerPrototypeSwitchActive_ ? "ON" : "OFF");
        ImGui::Text("Switch Source: Clone %s / Player %s", playerPrototypeCloneOnSwitch_ ? "ON" : "OFF", playerPrototypePlayerOnSwitch_ ? "ON" : "OFF");
        ImGui::Text("Door: %s", playerPrototypeDoorOpen_ ? "Open" : "Closed");
        ImGui::Text("Timed: Switch %s %.2f sec / Door %s", playerPrototypeTimedSwitchActive_ ? "ON" : "OFF", playerPrototypeTimedSwitch_.GetRemainingSeconds(), playerPrototypeTimedDoorOpen_ ? "Open" : "Closed");
        ImGui::Text("Toggle Lab: Switch %s  Clone %s / Gate %s / Lift %s Y %.2f Hold %.2f", playerPrototypeToggleSwitchActive_ ? "ON" : "OFF", playerPrototypeToggleSwitchCloneOn_ ? "ON" : "OFF", playerPrototypeToggleGateOpen_ ? "Open" : "Closed", playerPrototypeToggleElevator_.IsWaitingAtEndpoint() ? "Holding" : (playerPrototypeToggleElevatorActive_ ? "Moving" : "Idle"), playerPrototypeToggleElevator_.GetCurrentTranslate().y, playerPrototypeToggleElevator_.GetEndpointWaitRemainingSeconds());
        ImGui::Text("Weight: %s  Player %s / Clone %s  Bridge %s", playerPrototypeWeightSwitchActive_ ? "ON" : "OFF", playerPrototypeWeightPlayerOn_ ? "ON" : "OFF", playerPrototypeWeightCloneOn_ ? "ON" : "OFF", playerPrototypeGoalBridgeDeployed_ ? "Deployed" : "Retracted");
        ImGui::Text("OneWay: %s", playerPrototypeOneWayGateBlocking_ ? "Blocking" : "Passable");
        ImGui::Text("Route: Multi-clone main  Takes: %u  Stored: %zu / %zu",
            playerPrototypeRecordTakeCount_, storedCloneCount, playerPrototypeStageRules_.maxStoredClones);
    }
    ImGui::Text("Time: %.2f sec  Clear: %.2f sec  Record: %.2f sec", playerPrototypeElapsedTime_, playerPrototypeClearTime_, playerPrototypeLastRecordDuration_);
    if (pastSelfRecorder_.IsRecording() || !pastSelfRecorder_.GetFrames().empty()) {
        const float recordDuration = pastSelfRecorder_.GetDuration(); // 記録ゲージへ表示する現在の記録時間
        const float maximumRecordDuration = pastSelfRecorder_.GetMaxRecordTime(); // 記録ゲージの最大時間
        const float recordProgress = maximumRecordDuration > 0.0f
            ? std::clamp(recordDuration / maximumRecordDuration, 0.0f, 1.0f)
            : 0.0f; // 最大時間に対する現在の記録進捗率
        char recordProgressText[64] {}; // 記録ゲージ上に表示する状態と時間
        std::snprintf(recordProgressText, sizeof(recordProgressText), "%s %.2f / %.2f sec",
            pastSelfRecorder_.IsRecording() ? "Recording" : "Recorded", recordDuration, maximumRecordDuration);
        const ImVec4 recordProgressColor = pastSelfRecorder_.IsRecording()
            ? ImVec4(1.0f, 0.3f, 0.08f, 1.0f)
            : ImVec4(0.0f, 0.75f, 1.0f, 1.0f); // 記録中と記録済みを区別するゲージ色
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, recordProgressColor);
        ImGui::ProgressBar(recordProgress, ImVec2(-1.0f, 0.0f), recordProgressText);
        ImGui::PopStyleColor();
    }
    if (playerPrototypeGoalReached_) {
        ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.25f, 1.0f), "CLEAR");
    }
    ImGui::Text("Goal: %s", playerPrototypeGoalReached_ ? "Reached" : "Not Reached");
    if (cloneSlotsFull && !pastSelfRecorder_.IsRecording() && !playerPrototypeGoalReached_) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Clone slots full: press X to remove the last clone");
    }
    const bool canPrepareReplay = !playerPrototypeGoalReached_ && !pastSelfRecorder_.IsRecording() &&
        storedCloneCount > 0; // 分身群を残して再生準備へ戻せるか
    ImGui::Text("Prepare: %s", playerPrototypeGoalReached_ ? "Locked - reset puzzle to restart" :
        (canPrepareReplay ? "Ready - keeps stored clones" : "Locked - store a clone and stop recording"));
    if (playerPrototypePrepareFeedbackSeconds_ > 0.0f) {
        ImGui::TextColored(ImVec4(0.15f, 1.0f, 0.45f, 1.0f), "PREPARED: clones kept / replay ready");
    }
    if (playerPrototypeGoalReached_) {
        ImGui::BeginDisabled();
    }
    const bool disableRecordingButton = !pastSelfRecorder_.IsRecording() && !CanStartPlayerPrototypeRecording(); // 新規記録を開始できず停止操作でもないか
    if (disableRecordingButton) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(pastSelfRecorder_.IsRecording() ? "Stop Recording" : "Record Next + Replay Stored")) {
        if (pastSelfRecorder_.IsRecording()) {
            FinishPlayerPrototypeRecording();
        } else {
            StartPlayerPrototypeRecording();
        }
    }
    if (disableRecordingButton) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Replay Stored Only")) {
        if (pastSelfCloneManager_.StartAll()) {
            playerPrototypeReplayStarted_ = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Clones")) {
        pastSelfCloneManager_.StopAll();
    }
    if (playerPrototypeGoalReached_) {
        ImGui::EndDisabled();
    }
    if (!canPrepareReplay) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Prepare Replay")) {
        ResetPlayerPrototypeReplayState();
    }
    if (!canPrepareReplay) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    const bool canUndoLastClone = !playerPrototypeGoalReached_ && !pastSelfRecorder_.IsRecording() &&
        storedCloneCount > 0; // 最後の分身を削除できるか
    if (!canUndoLastClone) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Undo Last Clone")) {
        UndoLastPlayerPrototypeClone();
    }
    if (!canUndoLastClone) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Puzzle")) {
        ResetPlayerPrototypeState();
    }
    if (ImGui::CollapsingHeader("Clone Identities", ImGuiTreeNodeFlags_DefaultOpen)) {
        pastSelfCloneManager_.DrawIdentityLegendImGui();
    }
    if (playerPrototypeShowVerificationDetails_) {
        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
            player_.DrawImGui();
        }
        if (ImGui::CollapsingHeader("Recorder", ImGuiTreeNodeFlags_DefaultOpen)) {
            pastSelfRecorder_.DrawImGui();
        }
        if (ImGui::CollapsingHeader("Clones", ImGuiTreeNodeFlags_DefaultOpen)) {
            pastSelfCloneManager_.DrawImGui();
        }
    }
#endif
}

/// <summary>
/// プレイヤー検証ルートの選択UIを表示する。
/// </summary>
void PlayScene::DrawPlayerPrototypeRouteSelector()
{
#ifdef USE_IMGUI
    ImGui::Text("Route Mode");
    const bool oneCloneSelected = playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::OneCloneTutorial; // 1体用ルートを選択中か
    if (ImGui::RadioButton("1 Clone", oneCloneSelected)) {
        ApplyPlayerPrototypeRouteMode(PlayerPrototypeRouteMode::OneCloneTutorial);
    }
    ImGui::SameLine();
    const bool twoCloneSelected = playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::TwoCloneTutorial; // 2体用ルートを選択中か
    if (ImGui::RadioButton("2 Clones", twoCloneSelected)) {
        ApplyPlayerPrototypeRouteMode(PlayerPrototypeRouteMode::TwoCloneTutorial);
    }
    ImGui::SameLine();
    const bool fullVerificationSelected = playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::FullVerification; // 総合ルートを選択中か
    if (ImGui::RadioButton("Full", fullVerificationSelected)) {
        ApplyPlayerPrototypeRouteMode(PlayerPrototypeRouteMode::FullVerification);
    }
#endif
}

/// <summary>
/// 選択された検証ルートに対応するルールを適用する。
/// </summary>
void PlayScene::ApplyPlayerPrototypeRouteMode(PlayerPrototypeRouteMode routeMode)
{
    if (playerPrototypeRouteMode_ == routeMode) {
        return;
    }

    playerPrototypeRouteMode_ = routeMode;
    switch (playerPrototypeRouteMode_) {
    case PlayerPrototypeRouteMode::OneCloneTutorial:
        playerPrototypeStageRules_.maxStoredClones = 1;
        break;
    case PlayerPrototypeRouteMode::TwoCloneTutorial:
        playerPrototypeStageRules_.maxStoredClones = 2;
        break;
    case PlayerPrototypeRouteMode::FullVerification:
        playerPrototypeStageRules_.maxStoredClones = 3;
        break;
    }
    pastSelfRecorder_.SetMaxRecordTime(playerPrototypeStageRules_.maxRecordTime);
    ResetPlayerPrototypeState();
}

/// <summary>
/// 現在選択中の検証ルート名を取得する。
/// </summary>
const char* PlayScene::GetPlayerPrototypeRouteLabel() const
{
    switch (playerPrototypeRouteMode_) {
    case PlayerPrototypeRouteMode::OneCloneTutorial:
        return "One-clone tutorial";
    case PlayerPrototypeRouteMode::TwoCloneTutorial:
        return "Two-clone tutorial";
    case PlayerPrototypeRouteMode::FullVerification:
        return "Full verification";
    }
    return "Unknown route";
}

/// <summary>
/// 現在の攻略状態から次に行う確認手順を取得する。
/// </summary>
const char* PlayScene::GetPlayerPrototypeNextActionText() const
{
    const size_t storedCloneCount = pastSelfCloneManager_.GetCloneCount(); // 保存済みの分身数
    const size_t visibleCloneCount = pastSelfCloneManager_.GetVisibleCount(); // 表示中の分身数
    const bool hasStoredClones = storedCloneCount > 0; // 再生に使用できる分身があるか
    if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::OneCloneTutorial) {
        if (playerPrototypeOneCloneTutorialComplete_) {
            return "One-clone tutorial route complete";
        }
        if (pastSelfRecorder_.IsRecording()) {
            return "Record the clone moving onto the orange toggle";
        }
        if (!hasStoredClones) {
            return "Press C and record one clone on the orange toggle";
        }
        if (!playerPrototypePrepareUsed_) {
            return "Press T to prepare the one-clone replay";
        }
        if (visibleCloneCount == 0) {
            return "Press V to replay the stored clone";
        }
        if (!playerPrototypeOneCloneToggleActivated_) {
            return "Wait for the clone to activate the orange toggle";
        }
        if (!playerPrototypeOneCloneElevatorRidden_) {
            return "Move left and board the orange moving lift";
        }
        return "Ride the orange moving lift to the upper endpoint";
    }
    if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::TwoCloneTutorial) {
        if (playerPrototypeTwoCloneTutorialComplete_) {
            return "Two-clone tutorial route complete";
        }
        if (pastSelfRecorder_.IsRecording()) {
            return storedCloneCount == 0 ? "Record the first clone role" : "Record the second clone role";
        }
        if (storedCloneCount < 2) {
            return storedCloneCount == 0 ? "Press C to record the first clone role" : "Press T, then C to record the second clone role";
        }
        if (!playerPrototypeTwoCloneReplayPrepared_) {
            return "Press T to prepare the two-clone replay";
        }
        if (visibleCloneCount == 0) {
            return "Press V to replay both stored clones";
        }
        if (!playerPrototypeTwoCloneSwitchesActivated_) {
            return "Keep separate clones on the green and blue switches";
        }
        return "Move right through the opened blue door";
    }
    const int completedCheckCount = (playerPrototypeDoorBlockedBeforeClone_ ? 1 : 0) +
        (playerPrototypeDoorOpenedByClone_ ? 1 : 0) +
        (playerPrototypeClonePlatformUsed_ ? 1 : 0) +
        (playerPrototypeTimedDoorOpened_ ? 1 : 0) +
        (playerPrototypeWeightSwitchActivated_ ? 1 : 0) +
        (playerPrototypeOneWayGateUsed_ ? 1 : 0) +
        (playerPrototypeGoalReached_ ? 1 : 0); // 達成済みの検証項目数
    const int completedFlowCount = (playerPrototypeResetShown_ ? 1 : 0) +
        (playerPrototypeRecordStarted_ ? 1 : 0) +
        (playerPrototypeRecordStopped_ ? 1 : 0) +
        (playerPrototypePrepareUsed_ ? 1 : 0) +
        (playerPrototypeReplayStarted_ ? 1 : 0); // 達成済みの動画操作項目数
    const bool allChecksComplete = completedCheckCount == 7; // すべての検証項目を達成したか
    const bool videoFlowComplete = completedFlowCount == 5; // 動画操作項目をすべて達成したか
    const bool enoughStoredClones = storedCloneCount >= 2; // 複数分身ルートに必要な記録数があるか
    const bool multiCloneRouteComplete = allChecksComplete && videoFlowComplete && enoughStoredClones &&
        playerPrototypeDualCloneSwitchesActivated_; // 複数分身ルートを完了したか

    if (playerPrototypeGoalReached_) {
        if (multiCloneRouteComplete) {
            return "Multi-clone route complete";
        }
        if (!allChecksComplete) {
            return "Goal reached; verification checks still missing";
        }
        if (!videoFlowComplete) {
            return "Goal reached; video proof flow still missing";
        }
        if (!enoughStoredClones) {
            return "Goal reached; store 2 or more clones";
        }
        return "Goal reached; multi-clone proof still missing";
    }
    if (pastSelfRecorder_.IsRecording()) {
        return "Recording next clone; stored clones replay automatically";
    }
    if (playerPrototypeStageRules_.maxStoredClones == 0) {
        return "No clone slots configured for this stage";
    }
    if (!hasStoredClones) {
        return "Press C to record the first clone role";
    }
    if (!playerPrototypePrepareUsed_) {
        return "Press T to prepare replay with records kept";
    }
    if (!playerPrototypeDoorBlockedBeforeClone_) {
        return "Show the closed green door blocks the player";
    }
    if (visibleCloneCount == 0) {
        return "Press V to replay stored clones only (no new recording)";
    }
    if (playerPrototypePlayerOnSwitch_ && !playerPrototypeCloneOnSwitch_) {
        return "Move the clone onto the green switch";
    }
    if (!playerPrototypeDoorOpenedByClone_) {
        return "Wait for the clone to open the green door";
    }
    if (!playerPrototypeClonePlatformUsed_) {
        return "Use a clone as the blue-step platform";
    }
    if (!playerPrototypeDualCloneSwitchesActivated_) {
        return "Keep clones on the green and blue switches";
    }
    if (!playerPrototypeTimedDoorOpened_) {
        return "Pass through the opened blue door";
    }
    if (!playerPrototypeWeightSwitchActivated_) {
        return "Activate the yellow switch with player and clone";
    }
    if (!playerPrototypeOneWayGateUsed_) {
        return "Pass the purple gate and test the return path";
    }
    return "Reach the goal";
}

/// <summary>
/// 新たに達成した検証項目をHUD通知へ登録する。
/// </summary>
void PlayScene::RegisterPlayerPrototypeCheckCompleted(const char* checkText)
{
    if (!checkText || checkText[0] == '\0') {
        return;
    }

    playerPrototypeRecentCheckText_ = checkText;
    playerPrototypeRecentCheckSeconds_ = kCompletedCheckFeedbackDuration;
}

/// <summary>
/// プレイヤー操作に必要な主要状態を固定表示する。
/// </summary>
void PlayScene::DrawPlayerPrototypeFixedStatusHud()
{
#ifdef USE_IMGUI
    const ImVec4 checkedColor = ImVec4(0.15f, 1.0f, 0.45f, 1.0f); // 達成済み状態の表示色
    const ImVec4 uncheckedColor = ImVec4(1.0f, 0.35f, 0.25f, 1.0f); // 未達成状態の表示色
    const size_t storedCloneCount = pastSelfCloneManager_.GetCloneCount(); // 保存済みの分身数
    const size_t visibleCloneCount = pastSelfCloneManager_.GetVisibleCount(); // 表示中の分身数
    const size_t playingCloneCount = pastSelfCloneManager_.GetPlayingCount(); // 再生中の分身数
    const int completedCheckCount = (playerPrototypeDoorBlockedBeforeClone_ ? 1 : 0) +
        (playerPrototypeDoorOpenedByClone_ ? 1 : 0) +
        (playerPrototypeClonePlatformUsed_ ? 1 : 0) +
        (playerPrototypeTimedDoorOpened_ ? 1 : 0) +
        (playerPrototypeWeightSwitchActivated_ ? 1 : 0) +
        (playerPrototypeOneWayGateUsed_ ? 1 : 0) +
        (playerPrototypeGoalReached_ ? 1 : 0); // 達成済みの検証項目数
    const int completedFlowCount = (playerPrototypeResetShown_ ? 1 : 0) +
        (playerPrototypeRecordStarted_ ? 1 : 0) +
        (playerPrototypeRecordStopped_ ? 1 : 0) +
        (playerPrototypePrepareUsed_ ? 1 : 0) +
        (playerPrototypeReplayStarted_ ? 1 : 0); // 達成済みの動画操作項目数
    const bool allChecksComplete = completedCheckCount == 7; // 検証項目をすべて達成したか
    const bool videoFlowComplete = completedFlowCount == 5; // 動画操作項目をすべて達成したか
    const bool multiCloneRouteComplete = allChecksComplete && videoFlowComplete && playerPrototypeGoalReached_ &&
        storedCloneCount >= 2 && playerPrototypeDualCloneSwitchesActivated_; // 複数分身ルートを完了したか
    const bool oneCloneRecordingStored = playerPrototypeRecordTakeCount_ == 1 && storedCloneCount == 1 &&
        playerPrototypeRecordStopped_; // 1体用ルートに必要な記録を1回だけ保存したか
    const int oneCloneTutorialCheckCount = (oneCloneRecordingStored ? 1 : 0) +
        (playerPrototypeOneCloneToggleActivated_ ? 1 : 0) +
        (playerPrototypeOneCloneElevatorRidden_ ? 1 : 0) +
        (playerPrototypeOneCloneTutorialComplete_ ? 1 : 0); // 1体用ルートの達成済み項目数
    const bool twoCloneRecordingsStored = playerPrototypeRecordTakeCount_ == 2 && storedCloneCount == 2 &&
        playerPrototypeRecordStopped_; // 2体用ルートに必要な記録を2回保存したか
    const int twoCloneTutorialCheckCount = (twoCloneRecordingsStored ? 1 : 0) +
        (playerPrototypeTwoCloneReplayPrepared_ ? 1 : 0) +
        (playerPrototypeTwoCloneSwitchesActivated_ ? 1 : 0) +
        (playerPrototypeTwoCloneTutorialComplete_ ? 1 : 0); // 2体用ルートの達成済み項目数
    const char* nextActionText = GetPlayerPrototypeNextActionText(); // 通常表示でも確認できる次の攻略手順
    const float summaryLineCount = playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::FullVerification ? 8.0f : 6.0f; // 選択ルートに必要な固定表示行数
    const float summaryHeight = ImGui::GetTextLineHeightWithSpacing() * summaryLineCount +
        ImGui::GetStyle().WindowPadding.y * 2.0f; // 固定サマリー領域の高さ

    ImGui::BeginChild("PlayerFixedStatus", ImVec2(0.0f, summaryHeight), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::Text("%s", GetPlayerPrototypeRouteLabel());
    ImGui::SameLine();
    ImGui::Text("Record: %s", pastSelfRecorder_.IsRecording() ? "Recording" : "Stopped");
    ImGui::Text("Takes: %u  Stored: %zu/%zu  Visible: %zu  Playing: %zu",
        playerPrototypeRecordTakeCount_, storedCloneCount, playerPrototypeStageRules_.maxStoredClones,
        visibleCloneCount, playingCloneCount);
    if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::OneCloneTutorial) {
        ImGui::TextColored(playerPrototypeOneCloneTutorialComplete_ ? checkedColor : uncheckedColor,
            "One-clone tutorial: %d / 4%s", oneCloneTutorialCheckCount,
            playerPrototypeOneCloneTutorialComplete_ ? " Complete" : "");
        ImGui::TextWrapped("Next: %s", nextActionText);
        ImGui::EndChild();
        return;
    }
    if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::TwoCloneTutorial) {
        ImGui::TextColored(playerPrototypeTwoCloneTutorialComplete_ ? checkedColor : uncheckedColor,
            "Two-clone tutorial: %d / 4%s", twoCloneTutorialCheckCount,
            playerPrototypeTwoCloneTutorialComplete_ ? " Complete" : "");
        ImGui::TextWrapped("Next: %s", nextActionText);
        ImGui::EndChild();
        return;
    }
    if (playerPrototypeGoalReached_) {
        ImGui::TextColored(checkedColor, "Goal Reached / CLEAR");
        ImGui::TextColored(allChecksComplete ? checkedColor : uncheckedColor, "%d / 7%s",
            completedCheckCount, allChecksComplete ? " All complete" : " incomplete");
        ImGui::SameLine();
        ImGui::TextColored(videoFlowComplete ? checkedColor : uncheckedColor, "Video: %d / 5%s",
            completedFlowCount, videoFlowComplete ? " Flow complete" : "");
        ImGui::TextColored(multiCloneRouteComplete ? checkedColor : uncheckedColor, "%s",
            multiCloneRouteComplete ? "Multi-clone route complete" : nextActionText);
    } else {
        ImGui::TextColored(uncheckedColor, "Goal: Not Reached");
        ImGui::SameLine();
        ImGui::TextColored(uncheckedColor, "Route: Incomplete");
        if (playerPrototypeShowVerificationDetails_) {
            ImGui::TextColored(allChecksComplete ? checkedColor : uncheckedColor, "Checks: %d / 7%s",
                completedCheckCount, allChecksComplete ? " All complete" : "");
            ImGui::SameLine();
            ImGui::TextColored(videoFlowComplete ? checkedColor : uncheckedColor, "Video: %d / 5%s",
                completedFlowCount, videoFlowComplete ? " Flow complete" : "");
        } else {
            ImGui::Text("Progress: %d / 7", completedCheckCount);
            ImGui::SameLine();
            ImGui::TextDisabled("Mode: Play");
        }
        if (playerPrototypeRecentCheckSeconds_ > 0.0f && !playerPrototypeRecentCheckText_.empty()) {
            ImGui::TextColored(checkedColor, "Completed: %s", playerPrototypeRecentCheckText_.c_str());
        } else {
            ImGui::TextWrapped("Next: %s", nextActionText);
        }
    }
    ImGui::EndChild();
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
    const ImVec4 normalDoorColor = ImVec4(0.0f, 1.0f, 0.36f, 1.0f); // 通常扉と分身スイッチの案内色
    const ImVec4 timedDoorColor = ImVec4(0.0f, 0.7f, 1.0f, 1.0f); // 時間差扉と時間差スイッチの案内色
    const ImVec4 toggleSwitchColor = ImVec4(1.0f, 0.45f, 0.08f, 1.0f); // トグルスイッチと連動ゲートの案内色
    const ImVec4 weightSwitchColor = ImVec4(1.0f, 0.9f, 0.12f, 1.0f); // 重さスイッチの案内色
    const ImVec4 oneWayGateColor = ImVec4(0.9f, 0.05f, 1.0f, 1.0f); // 一方通行ゲートの案内色
    const char* recordStateLabel = pastSelfRecorder_.IsRecording() ? "Recording" : "Stopped"; // 記録状態表示
    const size_t storedCloneCount = pastSelfCloneManager_.GetCloneCount(); // 保存済みの分身数
    const size_t visibleCloneCount = pastSelfCloneManager_.GetVisibleCount(); // 表示中の分身数
    const size_t playingCloneCount = pastSelfCloneManager_.GetPlayingCount(); // 再生中の分身数
    const bool hasStoredClones = storedCloneCount > 0; // 同時再生に使用できる分身があるか
    const bool canPrepareReplay = !pastSelfRecorder_.IsRecording() && hasStoredClones; // Prepare操作を受け付けられるか
    const char* routeLabel = GetPlayerPrototypeRouteLabel(); // 現在の検証ルート種別
    const char* routeNeedText = "Stored >= 2 and Green + Blue active together"; // 選択ルートの主要完了条件
    if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::OneCloneTutorial) {
        routeNeedText = "Takes 1 / Stored 1 / Orange toggle + moving lift";
    } else if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::TwoCloneTutorial) {
        routeNeedText = "Takes 2 / Stored 2 / Green + Blue / pass blue door";
    }
    const char* phaseLabel = "Reset / no record"; // 現在の検証フェーズ表示
    const int completedCheckCount = (playerPrototypeDoorBlockedBeforeClone_ ? 1 : 0) +
        (playerPrototypeDoorOpenedByClone_ ? 1 : 0) +
        (playerPrototypeClonePlatformUsed_ ? 1 : 0) +
        (playerPrototypeTimedDoorOpened_ ? 1 : 0) +
        (playerPrototypeWeightSwitchActivated_ ? 1 : 0) +
        (playerPrototypeOneWayGateUsed_ ? 1 : 0) +
        (playerPrototypeGoalReached_ ? 1 : 0); // 動画確認用の達成済み項目数
    const int completedFlowCount = (playerPrototypeResetShown_ ? 1 : 0) +
        (playerPrototypeRecordStarted_ ? 1 : 0) +
        (playerPrototypeRecordStopped_ ? 1 : 0) +
        (playerPrototypePrepareUsed_ ? 1 : 0) +
        (playerPrototypeReplayStarted_ ? 1 : 0); // 動画確認用の操作フロー達成数
    const bool allChecksComplete = completedCheckCount == 7; // すべての検証項目を達成したか
    const bool videoFlowComplete = completedFlowCount == 5; // 撮影で必要な操作フローを満たしたか
    const bool enoughStoredClones = storedCloneCount >= 2; // 複数分身を使ったルートとして扱えるか
    const bool multiCloneRouteComplete = allChecksComplete && videoFlowComplete && playerPrototypeGoalReached_ && enoughStoredClones && playerPrototypeDualCloneSwitchesActivated_; // 複数分身を使う正式ルートとして完了したか
    if (playerPrototypeGoalReached_) {
        phaseLabel = "Goal reached";
    } else if (pastSelfRecorder_.IsRecording()) {
        phaseLabel = "Recording";
    } else if (hasStoredClones && visibleCloneCount == 0) {
        phaseLabel = "Replay ready after Prepare";
    } else if (playingCloneCount > 0) {
        phaseLabel = "Clones playing";
    } else if (visibleCloneCount > 0) {
        phaseLabel = "Clones finished";
    }

    const char* nextActionText = GetPlayerPrototypeNextActionText(); // HUDに表示する次の確認手順

    ImGui::Text("Prototype Verify");
    ImGui::Separator();
    ImGui::TextWrapped("C Record next + replay stored | V Replay stored only | B Stop clones");
    ImGui::TextWrapped("T Prepare replay | X Undo last clone | R Reset puzzle");
    ImGui::Text("Phase : %s", phaseLabel);
    ImGui::Text("Time  : %.2f sec  Clear %.2f sec", playerPrototypeElapsedTime_, playerPrototypeClearTime_);
    ImGui::Text("Route : %s  Takes: %u", routeLabel, playerPrototypeRecordTakeCount_);
    ImGui::Text("Need  : %s", routeNeedText);
    ImGui::Text("Record: %s  Frames: %zu  %.2f sec", recordStateLabel, pastSelfRecorder_.GetFrames().size(), playerPrototypeLastRecordDuration_);
    ImGui::Text("Clones: Stored %zu/%zu  Visible %zu  Playing %zu", storedCloneCount,
        playerPrototypeStageRules_.maxStoredClones, visibleCloneCount, playingCloneCount);
    ImGui::Text("Prepare: %s", playerPrototypeGoalReached_ ? "Locked after clear" : (canPrepareReplay ? "Ready" : "Locked"));
    if (playerPrototypePrepareFeedbackSeconds_ > 0.0f) {
        ImGui::TextColored(checkedColor, "PREPARED: start position / records kept");
    }
    ImGui::Text("Switch: %s  Clone %s  Player %s",
        playerPrototypeSwitchActive_ ? "ON" : "OFF",
        playerPrototypeCloneOnSwitch_ ? "ON" : "OFF",
        playerPrototypePlayerOnSwitch_ ? "ON" : "OFF");
    ImGui::Text("Door  : %s  Timed %s  Goal %s%s", playerPrototypeDoorOpen_ ? "Open" : "Closed", playerPrototypeTimedDoorOpen_ ? "Open" : "Closed", playerPrototypeGoalReached_ ? "Reached" : "Not Reached", playerPrototypeGoalReached_ ? " / CLEAR" : "");
    ImGui::Text("Timed: %s  Clone %s  %.2f sec", playerPrototypeTimedSwitchActive_ ? "ON" : "OFF", playerPrototypeTimedSwitchCloneOn_ ? "ON" : "OFF", playerPrototypeTimedSwitch_.GetRemainingSeconds());
    ImGui::Text("Toggle: %s  Clone %s  Gate %s  Lift %s Y %.2f Hold %.2f", playerPrototypeToggleSwitchActive_ ? "ON" : "OFF", playerPrototypeToggleSwitchCloneOn_ ? "ON" : "OFF", playerPrototypeToggleGateOpen_ ? "Open" : "Closed", playerPrototypeToggleElevator_.IsWaitingAtEndpoint() ? "Holding" : (playerPrototypeToggleElevatorActive_ ? "Moving" : "Idle"), playerPrototypeToggleElevator_.GetCurrentTranslate().y, playerPrototypeToggleElevator_.GetEndpointWaitRemainingSeconds());
    ImGui::Text("Weight: %s  Player %s  Clone %s  Bridge %s", playerPrototypeWeightSwitchActive_ ? "ON" : "OFF", playerPrototypeWeightPlayerOn_ ? "ON" : "OFF", playerPrototypeWeightCloneOn_ ? "ON" : "OFF", playerPrototypeGoalBridgeDeployed_ ? "Deployed" : "Retracted");
    ImGui::Text("OneWay: %s", playerPrototypeOneWayGateBlocking_ ? "Blocking" : "Passable");
    ImGui::Text("Next  : %s", nextActionText);
    ImGui::Text("Checks: %d / 7%s", completedCheckCount, allChecksComplete ? " All complete" : "");
    ImGui::Text("Video : %d / 5%s", completedFlowCount, videoFlowComplete ? " Flow complete" : "");
    ImGui::TextColored(normalDoorColor, "Green : Clone switch door");
    ImGui::TextColored(timedDoorColor, "Blue  : Green + timed switch door");
    ImGui::TextColored(toggleSwitchColor, "Orange: Optional toggle gate + moving lift");
    ImGui::TextColored(weightSwitchColor, "Yellow: Weight switch + goal bridge");
    ImGui::TextColored(oneWayGateColor, "Purple: One-way return block");
    ImGui::Separator();
    ImGui::Text("Video proof flow");
    ImGui::TextColored(playerPrototypeResetShown_ ? checkedColor : uncheckedColor, "[%c] Reset state shown", playerPrototypeResetShown_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeRecordStarted_ ? checkedColor : uncheckedColor, "[%c] Recording started", playerPrototypeRecordStarted_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeRecordStopped_ ? checkedColor : uncheckedColor, "[%c] Recording stopped", playerPrototypeRecordStopped_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypePrepareUsed_ ? checkedColor : uncheckedColor, "[%c] Prepare returned with record kept", playerPrototypePrepareUsed_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeReplayStarted_ ? checkedColor : uncheckedColor, "[%c] Recorded clone replay started", playerPrototypeReplayStarted_ ? 'x' : ' ');
    ImGui::Separator();
    if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::OneCloneTutorial) {
        const bool oneCloneRecordingStored = playerPrototypeRecordTakeCount_ == 1 && storedCloneCount == 1 &&
            playerPrototypeRecordStopped_; // 1回の記録から分身を1体だけ保存したか
        ImGui::Text("One-clone tutorial");
        ImGui::TextColored(oneCloneRecordingStored ? checkedColor : uncheckedColor, "[%c] One recording stored", oneCloneRecordingStored ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeOneCloneToggleActivated_ ? checkedColor : uncheckedColor, "[%c] Clone activated orange toggle", playerPrototypeOneCloneToggleActivated_ ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeOneCloneElevatorRidden_ ? checkedColor : uncheckedColor, "[%c] Player rode orange moving lift", playerPrototypeOneCloneElevatorRidden_ ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeOneCloneTutorialComplete_ ? checkedColor : uncheckedColor, "[%c] Upper endpoint reached", playerPrototypeOneCloneTutorialComplete_ ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeOneCloneTutorialComplete_ ? checkedColor : uncheckedColor, "%s",
            playerPrototypeOneCloneTutorialComplete_ ? "One-clone tutorial route complete" : "One-clone tutorial route incomplete");
        ImGui::Separator();
    }
    if (playerPrototypeRouteMode_ == PlayerPrototypeRouteMode::TwoCloneTutorial) {
        const bool twoCloneRecordingsStored = playerPrototypeRecordTakeCount_ == 2 && storedCloneCount == 2 &&
            playerPrototypeRecordStopped_; // 2回の記録から分身を2体だけ保存したか
        ImGui::Text("Two-clone tutorial");
        ImGui::TextColored(twoCloneRecordingsStored ? checkedColor : uncheckedColor, "[%c] Two recordings stored", twoCloneRecordingsStored ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeTwoCloneReplayPrepared_ ? checkedColor : uncheckedColor, "[%c] Prepare kept two records", playerPrototypeTwoCloneReplayPrepared_ ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeTwoCloneSwitchesActivated_ ? checkedColor : uncheckedColor, "[%c] Separate clones activated Green + Blue", playerPrototypeTwoCloneSwitchesActivated_ ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeTwoCloneTutorialComplete_ ? checkedColor : uncheckedColor, "[%c] Player passed blue door", playerPrototypeTwoCloneTutorialComplete_ ? 'x' : ' ');
        ImGui::TextColored(playerPrototypeTwoCloneTutorialComplete_ ? checkedColor : uncheckedColor, "%s",
            playerPrototypeTwoCloneTutorialComplete_ ? "Two-clone tutorial route complete" : "Two-clone tutorial route incomplete");
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Implementation Proof", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Runtime-owned classes");
        ImGui::BulletText("Clone manager: %s", typeid(pastSelfCloneManager_).name());
        ImGui::BulletText("Stored units : %s x %zu", typeid(PastSelfClone).name(), storedCloneCount);
        ImGui::BulletText("Green route : %s -> %s",
            typeid(playerPrototypeSwitch_).name(), typeid(playerPrototypeDoor_).name());
        ImGui::BulletText("Blue route  : %s + %s",
            typeid(playerPrototypeSwitch_).name(), typeid(playerPrototypeTimedSwitch_).name());
        ImGui::BulletText("Toggle lab  : %s -> %s + %s",
            typeid(playerPrototypeToggleSwitch_).name(), typeid(playerPrototypeToggleGate_).name(), typeid(playerPrototypeToggleElevator_).name());
        ImGui::BulletText("Weight route: %s -> %s", typeid(playerPrototypeWeightSwitch_).name(), typeid(playerPrototypeGoalBridge_).name());
        ImGui::BulletText("Return route: %s", typeid(playerPrototypeOneWayGate_).name());
        ImGui::BulletText("Goal        : %s", typeid(playerPrototypeGoal_).name());
        ImGui::Text("Live connections");
        ImGui::BulletText("Green switch %s -> Linked door %s",
            playerPrototypeSwitchActive_ ? "ON" : "OFF", playerPrototypeDoorOpen_ ? "Open" : "Closed");
        ImGui::BulletText("Green %s + Timed %s -> Blue door %s",
            playerPrototypeSwitchActive_ ? "ON" : "OFF", playerPrototypeTimedSwitchActive_ ? "ON" : "OFF",
            playerPrototypeTimedDoorOpen_ ? "Open" : "Closed");
        ImGui::BulletText("Toggle %s / Clone %s -> Gate %s / Lift %s Y %.2f",
            playerPrototypeToggleSwitchActive_ ? "ON" : "OFF", playerPrototypeToggleSwitchCloneOn_ ? "ON" : "OFF",
            playerPrototypeToggleGateOpen_ ? "Open" : "Closed", playerPrototypeToggleElevatorActive_ ? "Moving" : "Idle",
            playerPrototypeToggleElevator_.GetCurrentTranslate().y);
        ImGui::BulletText("Player %s + Clone %s -> Weight %s -> Bridge %s",
            playerPrototypeWeightPlayerOn_ ? "ON" : "OFF", playerPrototypeWeightCloneOn_ ? "ON" : "OFF",
            playerPrototypeWeightSwitchActive_ ? "ON" : "OFF", playerPrototypeGoalBridgeDeployed_ ? "Deployed" : "Retracted");
        ImGui::BulletText("One-way %s / Goal %s",
            playerPrototypeOneWayGateBlocking_ ? "Blocking" : "Passable",
            playerPrototypeGoalReached_ ? "Reached" : "Not Reached");
        ImGui::TextDisabled("Source: application/player/PastSelfCloneManager.*");
        ImGui::TextDisabled("Source: application/gimmicks/StageGimmicks.*");
    }
    ImGui::Separator();
    ImGui::Text("Gimmick Debug");
    ImGui::Text("Units : BoxSwitch / LinkedDoor / TimedSwitch / ToggleSwitch / MovingPlatform / WeightSwitch / LinkedBridge / OneWayGate / Goal");
    ImGui::TextColored(normalDoorColor, "Green : BoxSwitch Clone %s -> LinkedDoor %s", playerPrototypeCloneOnSwitch_ ? "ON" : "OFF", playerPrototypeDoorOpen_ ? "Open" : "Closed");
    ImGui::TextColored(timedDoorColor, "Blue  : Green %s + TimedSwitch %s %.2f sec -> TimedDoor %s", playerPrototypeSwitchActive_ ? "ON" : "OFF", playerPrototypeTimedSwitchActive_ ? "ON" : "OFF", playerPrototypeTimedSwitch_.GetRemainingSeconds(), playerPrototypeTimedDoorOpen_ ? "Open" : "Closed");
    ImGui::TextColored(toggleSwitchColor, "Orange: ToggleSwitch %s Clone %s -> Gate %s / Lift %s Y %.2f Hold %.2f", playerPrototypeToggleSwitchActive_ ? "ON" : "OFF", playerPrototypeToggleSwitchCloneOn_ ? "ON" : "OFF", playerPrototypeToggleGateOpen_ ? "Open" : "Closed", playerPrototypeToggleElevator_.IsWaitingAtEndpoint() ? "Holding" : (playerPrototypeToggleElevatorActive_ ? "Moving" : "Idle"), playerPrototypeToggleElevator_.GetCurrentTranslate().y, playerPrototypeToggleElevator_.GetEndpointWaitRemainingSeconds());
    ImGui::TextColored(weightSwitchColor, "Yellow: WeightSwitch Player %s + Clone %s -> Bridge %s", playerPrototypeWeightPlayerOn_ ? "ON" : "OFF", playerPrototypeWeightCloneOn_ ? "ON" : "OFF", playerPrototypeGoalBridgeDeployed_ ? "Deployed" : "Retracted");
    ImGui::TextColored(oneWayGateColor, "Purple: OneWayGate %s", playerPrototypeOneWayGateBlocking_ ? "Return blocked" : "Passable");
    ImGui::Separator();
    ImGui::TextColored(playerPrototypeDoorBlockedBeforeClone_ ? checkedColor : uncheckedColor, "[%c] Closed door blocked player", playerPrototypeDoorBlockedBeforeClone_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeDoorOpenedByClone_ ? checkedColor : uncheckedColor, "[%c] Clone opened door switch", playerPrototypeDoorOpenedByClone_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeClonePlatformUsed_ ? checkedColor : uncheckedColor, "[%c] Player used clone as platform", playerPrototypeClonePlatformUsed_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeTimedDoorOpened_ ? checkedColor : uncheckedColor, "[%c] Green and timed switches opened blue door", playerPrototypeTimedDoorOpened_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeWeightSwitchActivated_ ? checkedColor : uncheckedColor, "[%c] Player and clone activated weight switch", playerPrototypeWeightSwitchActivated_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeOneWayGateUsed_ ? checkedColor : uncheckedColor, "[%c] One-way gate blocked return path", playerPrototypeOneWayGateUsed_ ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeGoalReached_ ? checkedColor : uncheckedColor, "[%c] Goal reached", playerPrototypeGoalReached_ ? 'x' : ' ');
    ImGui::Separator();
    ImGui::Text("Multi-clone proof");
    ImGui::TextColored(enoughStoredClones ? checkedColor : uncheckedColor, "[%c] Two or more clones stored", enoughStoredClones ? 'x' : ' ');
    ImGui::TextColored(playerPrototypeDualCloneSwitchesActivated_ ? checkedColor : uncheckedColor, "[%c] Separate clones activated Green + Blue", playerPrototypeDualCloneSwitchesActivated_ ? 'x' : ' ');
    if (playerPrototypeGoalReached_) {
        ImGui::Separator();
        ImGui::TextColored(checkedColor, "Goal Reached / CLEAR");
        ImGui::TextColored(allChecksComplete ? checkedColor : uncheckedColor, "%d / 7%s", completedCheckCount,
            allChecksComplete ? " All complete" : " incomplete");
        ImGui::TextColored(videoFlowComplete ? checkedColor : uncheckedColor, "%s", videoFlowComplete ? "Video flow complete" : "Video flow incomplete");
        ImGui::TextColored(multiCloneRouteComplete ? checkedColor : uncheckedColor, "%s", multiCloneRouteComplete ? "Multi-clone route complete" : "Multi-clone route incomplete");
        ImGui::TextColored(allChecksComplete ? checkedColor : uncheckedColor, "%s", allChecksComplete ? "All verification checks complete" : "Verification checks still missing");
        ImGui::TextColored(checkedColor, "Clear %.2f sec / Record %.2f sec", playerPrototypeClearTime_, playerPrototypeLastRecordDuration_);
    }
#endif
}
