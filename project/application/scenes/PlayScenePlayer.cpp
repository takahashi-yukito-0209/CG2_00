#include "PlayScene.h"
#include "PastSelfTutorialLayoutUtility.h"

#include "ImGuiManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/io/InputManager.h"
#include "../../engine/level/LevelWriter.h"
#include "../../engine/utility/FileUtility.h"
#include "../../engine/utility/JsonFileLoader.h"
#include "../../engine/utility/ResourceResolver.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <typeinfo>
#include <vector>

using namespace MyEngine;

namespace {
constexpr const char* kPastSelfTutorialModelFileName = "block/block.obj"; // チュートリアルプレイヤーに使用するモデル
constexpr const char* kPastSelfTutorialStageFileName = "levels/trace_shift_stage.json"; // 実ステージブロックの保存ファイル
constexpr uint32_t kPastSelfTutorialOneCloneRouteMask = 1u << 0; // 1体ルートで使用するブロックフラグ
constexpr uint32_t kPastSelfTutorialTwoCloneRouteMask = 1u << 1; // 2体ルートで使用するブロックフラグ
constexpr uint32_t kPastSelfTutorialFinalRouteMask = 1u << 2; // 最終ルートで使用するブロックフラグ
constexpr float kPastSelfTutorialCameraMinimumDistance = 24.0f; // 対象が近い時のカメラ最小距離
constexpr float kPastSelfTutorialCameraMaximumDistance = 44.0f; // 対象が離れた時のカメラ最大距離
constexpr Math::Vector3 kPastSelfTutorialCameraRotate = { -0.12f, 0.0f, 0.0f }; // 横視点に少し見下ろしを足したチュートリアル用カメラ回転
constexpr float kPastSelfTutorialCameraFovY = 0.62f; // 2.5Dチュートリアル用カメラ視野角
constexpr float kPastSelfTutorialCameraVisibleAspect = 1.15f; // 右側HUDを除いたゲーム表示領域として扱う横縦比
constexpr float kPastSelfTutorialCameraHorizontalPadding = 3.5f; // 対象範囲の左右に確保する余白
constexpr float kPastSelfTutorialCameraVerticalPadding = 2.5f; // 対象範囲の上下に確保する余白
constexpr float kPastSelfTutorialCameraFollowSpeed = 6.0f; // 注視点と距離を追従させる速度
constexpr Math::Vector3 kPastSelfTutorialCameraFocusOffset = { 2.0f, 1.5f, 0.0f }; // 右側HUDを避けながら対象を画面内に収める注視点補正
constexpr uint8_t kRecordToggleKey = DIK_C; // 分身用記録の開始・停止キー
constexpr uint8_t kClonePlayKey = DIK_V; // 分身再生キー
constexpr uint8_t kCloneStopKey = DIK_B; // 分身停止キー
constexpr uint8_t kCloneUndoKey = DIK_X; // 最後に保存した分身の削除キー
constexpr uint8_t kReplayPrepareKey = DIK_T; // 記録を残したまま再生準備へ戻すキー
constexpr uint8_t kPastSelfTutorialResetKey = DIK_R; // 分身チュートリアルのリセットキー
constexpr float kPrepareFeedbackDuration = 1.5f; // Prepare成功表示を維持する秒数
constexpr float kCompletedCheckFeedbackDuration = 3.0f; // 検証項目の達成通知を表示する秒数
constexpr float kPastSelfTutorialFallResetY = -5.0f; // チュートリアルステージ外へ落ちたとみなすY座標
constexpr Math::Vector3 kPastSelfTutorialStartTranslate = { -8.7f, 0.5f, 0.0f }; // プレイヤー開始位置
constexpr Math::Vector4 kPastSelfTutorialNormalPlayerColor = { 0.0f, 0.86f, 1.0f, 1.0f }; // 通常時のプレイヤー色
constexpr Math::Vector4 kPastSelfTutorialRecordingPlayerColor = { 1.0f, 0.22f, 0.02f, 1.0f }; // 記録中のプレイヤー色
constexpr Math::Vector4 kPastSelfTutorialClearPlayerColor = { 1.0f, 0.88f, 0.12f, 1.0f }; // クリア時のプレイヤー色
constexpr Math::Vector3 kPastSelfTutorialCloneStartMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身開始地点マーカーの表示サイズ
constexpr Math::Vector3 kPastSelfTutorialCloneEndMarkerScale = { 0.7f, 0.7f, 2.2f }; // 分身終了地点マーカーの表示サイズ
constexpr Math::Vector4 kPastSelfTutorialCloneStartMarkerColor = { 0.0f, 0.86f, 1.0f, 0.5f }; // 分身開始地点マーカーの表示色
constexpr Math::Vector4 kPastSelfTutorialCloneEndMarkerColor = { 1.0f, 0.05f, 0.95f, 0.55f }; // 分身終了地点マーカーの表示色

struct PastSelfTutorialStageBlockDesc {
    Math::Vector3 scale; // ステージブロックの大きさ
    Math::Vector3 translate; // ステージブロックの中心位置
    Math::Vector4 color; // ステージブロックの表示色
    bool collidable; // 全面コライダーとして使うか
    bool goalMarker; // ゴール表示用のブロックか
};


struct PastSelfTutorialCameraFrame {
    Math::Vector3 focus; // カメラが追従する注視点
    float distance; // 対象範囲を収めるカメラ距離
};

constexpr std::array<PastSelfTutorialStageBlockDesc, 11> kPastSelfTutorialStageBlockDescs = { {
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
} }; // 各ギミックの作動状態を動画で読めるように間隔を取ったチュートリアルステージブロック
constexpr std::array<const char*, 11> kPastSelfTutorialStageBlockEditorLabels = {
    "Upper Goal Platform",
    "Left Start Platform",
    "Center Approach Platform",
    "Switch Approach Platform",
    "Switch Step",
    "Switch Guide Plate",
    "Door Exit Platform",
    "Timed Route Platform",
    "Upper Route Platform",
    "Goal Approach Platform",
    "Goal Marker",
}; // ステージ編集一覧へ表示するブロック名
constexpr Math::Vector3 kPastSelfTutorialGoalCenter = { 15.55f, 3.7f, 0.0f }; // ゴール判定の中心
constexpr Math::Vector3 kPastSelfTutorialGoalHalfSize = { 0.75f, 1.0f, 1.25f }; // ゴール判定の半サイズ
constexpr Math::Vector3 kPastSelfTutorialSwitchScale = { 2.0f, 0.12f, 2.7f }; // スイッチの表示サイズ
constexpr Math::Vector3 kPastSelfTutorialSwitchTranslate = { 1.2f, 1.31f, 0.0f }; // スイッチの中心座標
constexpr Math::Vector3 kPastSelfTutorialSwitchVolumeCenter = { 1.2f, 1.82f, 0.0f }; // スイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPastSelfTutorialSwitchVolumeHalfSize = { 1.15f, 0.72f, 1.55f }; // スイッチ入力を受ける範囲半サイズ
constexpr Math::Vector3 kPastSelfTutorialDoorScale = { 0.35f, 2.9f, 3.1f }; // 扉の表示サイズ
constexpr Math::Vector3 kPastSelfTutorialDoorTranslate = { 3.25f, 2.75f, 0.0f }; // 扉の中心座標
constexpr Math::Vector4 kPastSelfTutorialSwitchInactiveColor = { 0.03f, 0.16f, 0.08f, 1.0f }; // 押されていないスイッチ色
constexpr Math::Vector4 kPastSelfTutorialSwitchActiveColor = { 0.0f, 1.0f, 0.32f, 1.0f }; // 押されているスイッチ色
constexpr Math::Vector4 kPastSelfTutorialSwitchPlayerOnlyColor = { 1.0f, 0.62f, 0.12f, 1.0f }; // プレイヤーだけが乗っているスイッチ色
constexpr Math::Vector4 kPastSelfTutorialDoorClosedColor = { 0.0f, 0.55f, 0.18f, 1.0f }; // 閉じている扉色
constexpr Math::Vector4 kPastSelfTutorialDoorOpenColor = { 0.3f, 1.0f, 0.52f, 0.48f }; // 開いている扉色
constexpr Math::Vector4 kPastSelfTutorialGoalClearColor = { 1.0f, 0.88f, 0.12f, 1.0f }; // クリア済みのゴール色
constexpr Math::Vector3 kPastSelfTutorialTimedSwitchScale = { 1.7f, 0.12f, 2.5f }; // 時間差スイッチの表示サイズ
constexpr Math::Vector3 kPastSelfTutorialTimedSwitchTranslate = { 5.2f, 2.31f, 0.0f }; // 時間差スイッチの中心座標
constexpr Math::Vector3 kPastSelfTutorialTimedSwitchVolumeCenter = { 5.2f, 2.81f, 0.0f }; // 時間差スイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPastSelfTutorialTimedSwitchVolumeHalfSize = { 1.0f, 0.7f, 1.45f }; // 時間差スイッチ入力を受ける範囲半サイズ
constexpr Math::Vector3 kPastSelfTutorialTimedDoorScale = { 0.3f, 2.2f, 3.0f }; // 時間差扉の表示サイズ
constexpr Math::Vector3 kPastSelfTutorialTimedDoorTranslate = { 7.0f, 3.52f, 0.0f }; // 時間差扉の中心座標
constexpr Math::Vector4 kPastSelfTutorialTimedSwitchInactiveColor = { 0.02f, 0.1f, 0.32f, 1.0f }; // 時間差スイッチ未入力時の表示色
constexpr Math::Vector4 kPastSelfTutorialTimedSwitchActiveColor = { 0.0f, 0.82f, 1.0f, 1.0f }; // 時間差スイッチ起動中の表示色
constexpr Math::Vector4 kPastSelfTutorialTimedSwitchTriggerColor = { 0.2f, 1.0f, 1.0f, 1.0f }; // 時間差スイッチを分身が踏んでいる時の表示色
constexpr Math::Vector4 kPastSelfTutorialTimedDoorClosedColor = { 0.0f, 0.16f, 0.8f, 1.0f }; // 閉じている時間差扉色
constexpr Math::Vector4 kPastSelfTutorialTimedDoorOpenColor = { 0.3f, 0.86f, 1.0f, 0.5f }; // 開いている時間差扉色
constexpr float kPastSelfTutorialTimedSwitchHoldSeconds = 4.5f; // 時間差スイッチの起動維持秒数
constexpr Math::Vector3 kPastSelfTutorialToggleSwitchScale = { 1.2f, 0.12f, 2.4f }; // 1体用ルートのトグルスイッチ表示サイズ
constexpr Math::Vector3 kPastSelfTutorialToggleSwitchTranslate = { -7.75f, 0.06f, 0.0f }; // トグルスイッチの中心座標
constexpr Math::Vector3 kPastSelfTutorialToggleSwitchVolumeCenter = { -7.75f, 0.56f, 0.0f }; // トグルスイッチ入力範囲の中心
constexpr Math::Vector3 kPastSelfTutorialToggleSwitchVolumeHalfSize = { 0.65f, 0.7f, 1.4f }; // トグルスイッチ入力範囲の半サイズ
constexpr Math::Vector4 kPastSelfTutorialToggleSwitchInactiveColor = { 0.3f, 0.12f, 0.02f, 1.0f }; // トグルスイッチOFF時の表示色
constexpr Math::Vector4 kPastSelfTutorialToggleSwitchActiveColor = { 1.0f, 0.42f, 0.05f, 1.0f }; // トグルスイッチON時の表示色
constexpr Math::Vector4 kPastSelfTutorialToggleSwitchPressedColor = { 1.0f, 0.85f, 0.15f, 1.0f }; // トグルスイッチを分身が踏んでいる時の表示色
constexpr Math::Vector3 kPastSelfTutorialToggleGateScale = { 0.3f, 2.2f, 3.0f }; // 1体用ルートのトグル連動ゲート表示サイズ
constexpr Math::Vector3 kPastSelfTutorialToggleGateTranslate = { -11.5f, 1.1f, 0.0f }; // トグル連動ゲートの中心座標
constexpr Math::Vector4 kPastSelfTutorialToggleGateClosedColor = { 0.55f, 0.18f, 0.02f, 1.0f }; // トグル連動ゲート閉鎖時の表示色
constexpr Math::Vector4 kPastSelfTutorialToggleGateOpenColor = { 1.0f, 0.55f, 0.12f, 0.35f }; // トグル連動ゲート開放時の表示色
constexpr Math::Vector3 kPastSelfTutorialToggleElevatorScale = { 1.8f, 0.08f, 3.0f }; // 1体用ルートの昇降足場表示サイズ
constexpr Math::Vector3 kPastSelfTutorialToggleElevatorLowerTranslate = { -13.2f, 0.04f, 0.0f }; // 昇降足場の下端座標
constexpr Math::Vector3 kPastSelfTutorialToggleElevatorUpperTranslate = { -13.2f, 2.46f, 0.0f }; // 昇降足場の上端座標
constexpr Math::Vector4 kPastSelfTutorialToggleElevatorInactiveColor = { 0.45f, 0.18f, 0.02f, 1.0f }; // 昇降足場停止時の表示色
constexpr Math::Vector4 kPastSelfTutorialToggleElevatorActiveColor = { 1.0f, 0.55f, 0.08f, 1.0f }; // 昇降足場稼働時の表示色
constexpr float kPastSelfTutorialToggleElevatorMoveSpeed = 1.2f; // 昇降足場の1秒あたりの移動距離
constexpr float kPastSelfTutorialToggleElevatorUpperWaitSeconds = 1.0f; // 昇降足場が上端で停止する秒数
constexpr float kPastSelfTutorialToggleElevatorLowerWaitSeconds = 1.0f; // 昇降足場が下端で停止する秒数
constexpr Math::Vector3 kPastSelfTutorialWeightSwitchScale = { 1.45f, 0.12f, 2.4f }; // 重さスイッチの表示サイズ
constexpr Math::Vector3 kPastSelfTutorialWeightSwitchTranslate = { 11.4f, 2.76f, 0.0f }; // 重さスイッチの中心座標
constexpr Math::Vector3 kPastSelfTutorialWeightSwitchVolumeCenter = { 11.4f, 3.26f, 0.0f }; // 重さスイッチ入力を受ける範囲中心
constexpr Math::Vector3 kPastSelfTutorialWeightSwitchVolumeHalfSize = { 0.95f, 0.7f, 1.45f }; // 重さスイッチ入力を受ける範囲半サイズ
constexpr Math::Vector4 kPastSelfTutorialWeightSwitchInactiveColor = { 0.32f, 0.22f, 0.04f, 1.0f }; // 重さスイッチ未入力時の表示色
constexpr Math::Vector4 kPastSelfTutorialWeightSwitchPartialColor = { 1.0f, 0.62f, 0.12f, 1.0f }; // 重さスイッチ片方入力時の表示色
constexpr Math::Vector4 kPastSelfTutorialWeightSwitchActiveColor = { 1.0f, 0.9f, 0.12f, 1.0f }; // 重さスイッチ両方入力時の表示色
constexpr Math::Vector3 kPastSelfTutorialGoalBridgeScale = { 3.0f, 0.12f, 4.0f }; // ゴール前の連動橋の表示サイズ
constexpr Math::Vector3 kPastSelfTutorialGoalBridgeTranslate = { 14.2f, 2.64f, 0.0f }; // ゴール前の連動橋の中心座標
constexpr Math::Vector4 kPastSelfTutorialGoalBridgeRetractedColor = { 0.32f, 0.22f, 0.04f, 0.18f }; // 未展開時の連動橋色
constexpr Math::Vector4 kPastSelfTutorialGoalBridgeDeployedColor = { 1.0f, 0.9f, 0.12f, 1.0f }; // 展開時の連動橋色
constexpr Math::Vector3 kPastSelfTutorialOneWayGateScale = { 0.24f, 1.8f, 2.8f }; // 一方通行ゲートの表示サイズ
constexpr Math::Vector3 kPastSelfTutorialOneWayGateTranslate = { 13.2f, 3.55f, 0.0f }; // 終盤で戻りを塞ぐ一方通行ゲートの中心座標
constexpr Math::Vector4 kPastSelfTutorialOneWayGatePassableColor = { 0.5f, 0.1f, 0.85f, 0.45f }; // 通行可能時の一方通行ゲート色
constexpr Math::Vector4 kPastSelfTutorialOneWayGateBlockingColor = { 1.0f, 0.05f, 1.0f, 1.0f }; // 戻りを塞ぐ時の一方通行ゲート色

/// <summary>
/// チュートリアルステージブロックの半サイズを計算する。
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
/// チュートリアルステージブロックから全面コライダー情報を作成する。
/// </summary>
SolidCollider BuildStageBlockSolidCollider(const PastSelfTutorialStageBlockDesc& blockDesc)
{
    SolidCollider collider {}; // チュートリアルステージから作成する全面コライダー
    collider.center = blockDesc.translate;
    collider.halfSize = CalculateStageBlockHalfSize(blockDesc.scale);
    collider.enabled = blockDesc.collidable;
    return collider;
}

/// <summary>
/// チュートリアルギミック用のブロックオブジェクトを作成する。
/// </summary>
std::unique_ptr<Object3d> CreatePastSelfTutorialBlockObject(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, uint32_t objectId, const Math::Vector3& scale, const Math::Vector3& translate, const Math::Vector4& color)
{
    std::unique_ptr<Object3d> object = std::make_unique<Object3d>(); // 作成するギミック表示オブジェクト
    object->SetObjectId(objectId);
    object->Initialize(object3dCommon, imguiManager);
    object->SetModel(kPastSelfTutorialModelFileName);
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
/// プレイヤー、可視分身、次のギミックを収めるカメラ範囲を計算する。
/// </summary>
PastSelfTutorialCameraFrame CalculatePastSelfTutorialCameraFrame(const PlayerState& playerState, const std::vector<PlayerState>& cloneStates, const Math::Vector3& targetPosition)
{
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
    focus.x += kPastSelfTutorialCameraFocusOffset.x;
    focus.y += kPastSelfTutorialCameraFocusOffset.y;
    focus.z = kPastSelfTutorialCameraFocusOffset.z;

    const float halfFovTangent = std::tan(kPastSelfTutorialCameraFovY * 0.5f); // 縦方向の表示範囲計算に使う視野角係数
    const float horizontalHalfRange = (maximumX - minimumX) * 0.5f + kPastSelfTutorialCameraHorizontalPadding; // 左右余白を含む半幅
    const float verticalHalfRange = (maximumY - minimumY) * 0.5f + kPastSelfTutorialCameraVerticalPadding; // 上下余白を含む半高
    const float horizontalDistance = horizontalHalfRange / (halfFovTangent * kPastSelfTutorialCameraVisibleAspect); // 横幅を収めるための距離
    const float verticalDistance = verticalHalfRange / halfFovTangent; // 高さを収めるための距離
    const float distance = std::clamp((std::max)(horizontalDistance, verticalDistance),
        kPastSelfTutorialCameraMinimumDistance, kPastSelfTutorialCameraMaximumDistance); // 使用範囲に制限したカメラ距離
    return { focus, distance };
}

/// <summary>
/// 現在値を目標値へフレーム時間に応じて追従させる。
/// </summary>
float FollowPastSelfTutorialCameraValue(float currentValue, float targetValue, float deltaTime)
{
    const float followRate = 1.0f - std::exp(-kPastSelfTutorialCameraFollowSpeed * (std::max)(deltaTime, 0.0f)); // フレームレートに依存しにくい追従率
    return currentValue + (targetValue - currentValue) * followRate;
}

/// <summary>
/// チュートリアル用カメラの回転を反映した座標を計算する。
/// </summary>
Math::Vector3 RotatePointForPastSelfTutorialCamera(const Math::Vector3& position)
{
    const float cosX = std::cos(kPastSelfTutorialCameraRotate.x); // X回転のcos値
    const float sinX = std::sin(kPastSelfTutorialCameraRotate.x); // X回転のsin値
    return {
        position.x,
        position.y * cosX - position.z * sinX,
        position.y * sinX + position.z * cosX
    };
}

/// <summary>
/// 2.5D用の横視点カメラをプレイヤー位置に合わせて設定する。
/// </summary>
void ConfigurePastSelfTutorialCamera(Camera* camera, const Math::Vector3& focus, float distance)
{
    if (!camera) {
        return;
    }

    const Math::Vector3 rotatedFocus = RotatePointForPastSelfTutorialCamera(focus); // ビュー回転後の注視点座標
    const Math::Vector3 cameraTranslate = {
        rotatedFocus.x,
        rotatedFocus.y,
        rotatedFocus.z - distance
    }; // 横視点で注視点を画面中央に置くカメラ位置
    camera->SetTranslate(cameraTranslate);
    camera->SetRotate(kPastSelfTutorialCameraRotate);
    camera->SetFovY(kPastSelfTutorialCameraFovY);
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
/// 分身チュートリアル用オブジェクトを初期化する。
/// </summary>
void PlayScene::InitializePastSelfTutorial()
{
    InitializePastSelfTutorialStage();
    InitializePastSelfTutorialMechanics();
    ApplyPastSelfTutorialGimmickLayouts();
    pastSelfRecorder_.SetMaxRecordTime(pastSelfTutorialStageRules_.maxRecordTime);
    player_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, kPastSelfTutorialModelFileName);
    player_.SetMaterialColor(kPastSelfTutorialNormalPlayerColor);
    PlayerState playerStartState = player_.GetState(); // チュートリアルステージに合わせた開始状態
    playerStartState.transform.translate = kPastSelfTutorialStartTranslate;
    player_.SetInitialState(playerStartState);
    pastSelfCloneManager_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, kPastSelfTutorialModelFileName);
    const PastSelfTutorialCameraFrame initialCameraFrame = CalculatePastSelfTutorialCameraFrame(
        player_.GetState(), {}, GetPastSelfTutorialRouteCameraTarget(player_.GetState())); // 初期位置と最初のギミックを収めるカメラ範囲
    pastSelfTutorialCameraFocus_ = initialCameraFrame.focus;
    pastSelfTutorialCameraDistance_ = initialCameraFrame.distance;
    ConfigurePastSelfTutorialCamera(ctx_.camera, pastSelfTutorialCameraFocus_, pastSelfTutorialCameraDistance_);
}

/// <summary>
/// 指定情報から実ステージブロックを構築して末尾へ追加する。
/// </summary>
size_t PlayScene::AppendPastSelfTutorialStageBlock(const std::string& name, const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& translate, const Math::Vector4& color, bool collidable, bool oneCloneGoalPlatform, bool goalMarker, uint32_t routeMask)
{
    PastSelfTutorialStageBlock stageBlock {}; // 追加する実ステージブロック
    stageBlock.name = name.empty() ? "Stage Block" : name;
    stageBlock.object = CreatePastSelfTutorialBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), scale, translate, color);
    stageBlock.object->SetRotate(rotate);
    stageBlock.collider.center = translate;
    stageBlock.collider.halfSize = CalculateStageBlockHalfSize(scale);
    stageBlock.collider.enabled = collidable;
    stageBlock.baseColor = color;
    stageBlock.routeMask = routeMask == 0 ? kPastSelfTutorialFinalRouteMask : routeMask;
    stageBlock.oneCloneGoalPlatform = oneCloneGoalPlatform;
    stageBlock.goalMarker = goalMarker;
    pastSelfTutorialStageBlocks_.push_back(std::move(stageBlock));
    return pastSelfTutorialStageBlocks_.size() - 1;
}

/// <summary>
/// 分身チュートリアル用ステージを初期化する。
/// </summary>
void PlayScene::InitializePastSelfTutorialStage()
{
    if (pastSelfTutorialStageFileName_.empty()) {
        pastSelfTutorialStageFileName_ = kPastSelfTutorialStageFileName;
    }
    pastSelfTutorialStageFilePath_ = LevelWriter::ResolveWritableLevelPath(pastSelfTutorialStageFileName_);
    if (!pastSelfTutorialStageFilePath_.empty() && LoadPastSelfTutorialStage()) {
        pastSelfTutorialState_.gimmicks.goalReached = false;
        ApplyPastSelfTutorialGoalVisual();
        return;
    }

    pastSelfTutorialStageBlocks_.clear();
    pastSelfTutorialGimmickLayouts_.clear();
    pastSelfTutorialStageBlocks_.reserve(kPastSelfTutorialStageBlockDescs.size());
    for (size_t blockIndex = 0; blockIndex < kPastSelfTutorialStageBlockDescs.size(); ++blockIndex) {
        const PastSelfTutorialStageBlockDesc& blockDesc = kPastSelfTutorialStageBlockDescs[blockIndex]; // フォールバック用の固定ブロック情報
        uint32_t routeMask = kPastSelfTutorialFinalRouteMask; // このブロックを使用するルート
        if (blockIndex <= 2) {
            routeMask |= kPastSelfTutorialOneCloneRouteMask;
        }
        if (blockIndex >= 2 && blockIndex <= 8) {
            routeMask |= kPastSelfTutorialTwoCloneRouteMask;
        }
        AppendPastSelfTutorialStageBlock(kPastSelfTutorialStageBlockEditorLabels[blockIndex], blockDesc.scale, { 0.0f, 0.0f, 0.0f }, blockDesc.translate, blockDesc.color, blockDesc.collidable, blockIndex == 0, blockDesc.goalMarker, routeMask);
    }

    pastSelfTutorialStageFileSucceeded_ = false;
    pastSelfTutorialStageFileMessage_ = "Stage JSON load failed. Using built-in fallback.";
    pastSelfTutorialState_.gimmicks.goalReached = false;
    ApplyPastSelfTutorialGoalVisual();
}

/// <summary>
/// 実ステージを再読み込みし、成功時は通算履歴を残して挑戦をリセットする。
/// </summary>
bool PlayScene::ReloadPastSelfTutorialStage()
{
    if (!LoadPastSelfTutorialStage()) {
        return false;
    }
    ConfigurePastSelfTutorialGoal();
    ApplyPastSelfTutorialGimmickLayouts();
    ResetPastSelfTutorialState();
    return true;
}

/// <summary>
/// JSONを検証して配置を置き換える。プレイヤーとギミックの状態は操作しない。
/// </summary>
bool PlayScene::LoadPastSelfTutorialStage()
{
    if (pastSelfTutorialStageFileName_.empty() || FileUtility::GetExtension(pastSelfTutorialStageFileName_) != ".json") {
        pastSelfTutorialStageFileSucceeded_ = false;
        pastSelfTutorialStageFileMessage_ = "Stage file must be a non-empty .json path.";
        return false;
    }
    pastSelfTutorialStageFilePath_ = LevelWriter::ResolveWritableLevelPath(pastSelfTutorialStageFileName_);

    ResourceResolver::ClearCache();
    JsonDocument root; // 読み込んだステージJSON
    std::string resolvedPath; // 実際に読み込んだファイルパス
    std::string loadError; // JSON読み込み失敗理由
    if (!JsonFileLoader::Load(pastSelfTutorialStageFilePath_, root, &resolvedPath, ResourceResolver::Type::Json, &loadError)) {
        pastSelfTutorialStageFileSucceeded_ = false;
        pastSelfTutorialStageFileMessage_ = loadError.empty() ? "Failed to load stage JSON." : loadError;
        return false;
    }
    TraceShiftStageJson::StageData loadedStage; // 描画オブジェクト生成前に検証する保存情報
    if (!TraceShiftStageJson::Decode(root, loadedStage, loadError)) {
        pastSelfTutorialStageFileSucceeded_ = false;
        pastSelfTutorialStageFileMessage_ = loadError;
        return false;
    }

    pastSelfTutorialStageBlocks_.clear();
    pastSelfTutorialStageBlocks_.reserve(loadedStage.blocks.size());
    for (const TraceShiftStageJson::BlockData& blockData : loadedStage.blocks) {
        AppendPastSelfTutorialStageBlock(blockData.name, blockData.scale, blockData.rotate, blockData.translate, blockData.color, blockData.collidable, blockData.oneCloneGoalPlatform, blockData.goalMarker, blockData.routeMask);
    }
    pastSelfTutorialGimmickLayouts_ = std::move(loadedStage.gimmicks);
    pastSelfTutorialStageFileSucceeded_ = true;
    pastSelfTutorialStageFileMessage_ = "Loaded stage JSON: " + resolvedPath;
    return true;
}

/// <summary>
/// 現在のゴール表示から判定範囲を設定する。
/// </summary>
void PlayScene::ConfigurePastSelfTutorialGoal()
{
    BoxGoalGimmickDesc goalDesc {}; // 現在のゴール表示に対応する判定情報
    goalDesc.center = kPastSelfTutorialGoalCenter;
    goalDesc.halfSize = kPastSelfTutorialGoalHalfSize;
    for (const PastSelfTutorialStageBlock& stageBlock : pastSelfTutorialStageBlocks_) {
        if (!stageBlock.goalMarker || !stageBlock.object) {
            continue;
        }
        const Math::Vector3 markerScale = stageBlock.object->GetScale(); // ゴール表示の現在のスケール
        goalDesc.center = stageBlock.object->GetTranslate();
        goalDesc.halfSize = PastSelfTutorialLayoutUtility::CalculateGoalHalfSize(
            markerScale, kPastSelfTutorialStageBlockDescs.back().scale, kPastSelfTutorialGoalHalfSize);
        break;
    }
    pastSelfTutorialGoal_.Configure(goalDesc);
}

/// <summary>
/// 現在の実ステージブロックとギミック配置をJSONへ保存する。
/// </summary>
bool PlayScene::SavePastSelfTutorialStage()
{
    if (pastSelfTutorialStageFileName_.empty() || FileUtility::GetExtension(pastSelfTutorialStageFileName_) != ".json") {
        pastSelfTutorialStageFileSucceeded_ = false;
        pastSelfTutorialStageFileMessage_ = "Stage file must be a non-empty .json path.";
        return false;
    }
    pastSelfTutorialStageFilePath_ = LevelWriter::ResolveWritableLevelPath(pastSelfTutorialStageFileName_);

    TraceShiftStageJson::StageData stageData; // 実オブジェクトから取得する保存用のスナップショット
    for (const PastSelfTutorialStageBlock& stageBlock : pastSelfTutorialStageBlocks_) {
        if (!stageBlock.object) {
            continue;
        }
        TraceShiftStageJson::BlockData block; // 固定ブロック1件分の保存情報
        block.name = stageBlock.name;
        block.scale = stageBlock.object->GetScale();
        block.rotate = stageBlock.object->GetRotate();
        block.translate = stageBlock.object->GetTranslate();
        block.color = stageBlock.baseColor;
        block.collidable = stageBlock.collider.enabled;
        block.oneCloneGoalPlatform = stageBlock.oneCloneGoalPlatform;
        block.goalMarker = stageBlock.goalMarker;
        block.routeMask = stageBlock.routeMask;
        stageData.blocks.push_back(std::move(block));
    }

    stageData.gimmicks = CollectPastSelfTutorialGimmickLayouts();
    std::string jsonText; // 検証後に書き込むステージJSON
    std::string saveError; // 検証またはファイル保存に失敗した理由
    if (!TraceShiftStageJson::Serialize(stageData, jsonText, saveError)) {
        pastSelfTutorialStageFileSucceeded_ = false;
        pastSelfTutorialStageFileMessage_ = "Failed to save stage JSON: " + pastSelfTutorialStageFilePath_ + " / " + saveError;
        return false;
    }
    const bool saveSucceeded = FileUtility::WriteText(pastSelfTutorialStageFilePath_, jsonText, &saveError); // 一時ファイル経由の書き込み結果
    pastSelfTutorialStageFileSucceeded_ = saveSucceeded;
    pastSelfTutorialStageFileMessage_ = saveSucceeded
        ? "Saved stage JSON: " + pastSelfTutorialStageFilePath_
        : "Failed to save stage JSON: " + pastSelfTutorialStageFilePath_ + " / " + saveError;
    if (saveSucceeded) {
        ResourceResolver::ClearCache();
    }
    return saveSucceeded;
}

/// <summary>
/// 現在のギミック配置を取得する。昇降足場は動作位置ではなく上下端を使用する。
/// </summary>
std::vector<PlayScene::PastSelfTutorialGimmickLayout> PlayScene::CollectPastSelfTutorialGimmickLayouts() const
{
    std::vector<PastSelfTutorialGimmickLayout> layouts; // 保存または初期配置の記録に使用する一覧
    const auto appendGimmick = [&layouts](const char* id, Object3d* object) {
        if (!object) {
            return;
        }
        PastSelfTutorialGimmickLayout layout; // 通常ギミック1件分の配置情報
        layout.id = id;
        layout.scale = object->GetScale();
        layout.rotate = object->GetRotate();
        layout.translate = object->GetTranslate();
        layouts.push_back(std::move(layout));
    }; // 表示オブジェクトから配置だけを取得する処理
    appendGimmick("clone_switch", pastSelfTutorialSwitch_.GetEditorObject());
    appendGimmick("linked_door", pastSelfTutorialDoor_.GetEditorObject());
    appendGimmick("timed_switch", pastSelfTutorialTimedSwitch_.GetEditorObject());
    appendGimmick("timed_door", pastSelfTutorialTimedDoor_.GetEditorObject());
    appendGimmick("toggle_switch", pastSelfTutorialToggleSwitch_.GetEditorObject());
    appendGimmick("toggle_gate", pastSelfTutorialToggleGate_.GetEditorObject());
    appendGimmick("weight_switch", pastSelfTutorialWeightSwitch_.GetEditorObject());
    appendGimmick("goal_bridge", pastSelfTutorialGoalBridge_.GetEditorObject());
    appendGimmick("one_way_gate", pastSelfTutorialOneWayGate_.GetEditorObject());

    Object3d* toggleElevatorObject = pastSelfTutorialToggleElevator_.GetEditorObject(); // 保存対象の昇降足場
    if (toggleElevatorObject) {
        PastSelfTutorialGimmickLayout layout; // 動作中の位置ではなく上下端を保存する配置情報
        layout.id = "toggle_elevator";
        layout.scale = toggleElevatorObject->GetScale();
        layout.rotate = toggleElevatorObject->GetRotate();
        layout.translate = pastSelfTutorialToggleElevator_.GetEditorLowerTranslate();
        layout.upperTranslate = pastSelfTutorialToggleElevator_.GetEditorUpperTranslate();
        layout.hasUpperTranslate = true;
        layouts.push_back(std::move(layout));
    }
    return layouts;
}

/// <summary>
/// 初期配置を基準に読み込んだギミック配置を実ステージへ反映する。
/// </summary>
void PlayScene::ApplyPastSelfTutorialGimmickLayouts()
{
    const auto applyObjectTransform = [](Object3d* object, const PastSelfTutorialGimmickLayout& layout) {
        if (!object) {
            return false;
        }
        object->SetScale(layout.scale);
        object->SetRotate(layout.rotate);
        object->SetTranslate(layout.translate);
        return true;
    }; // 通常ギミックの表示Transformを復元する処理

    const std::vector<PastSelfTutorialGimmickLayout> layouts = TraceShiftStageJson::ResolveGimmickLayouts(
        pastSelfTutorialDefaultGimmickLayouts_, pastSelfTutorialGimmickLayouts_); // 省略分を初期値で補った配置
    for (const PastSelfTutorialGimmickLayout& layout : layouts) {
        if (layout.id == "clone_switch") {
            if (applyObjectTransform(pastSelfTutorialSwitch_.GetEditorObject(), layout)) {
                pastSelfTutorialSwitch_.ApplyEditorTransform();
            }
        } else if (layout.id == "linked_door") {
            if (applyObjectTransform(pastSelfTutorialDoor_.GetEditorObject(), layout)) {
                pastSelfTutorialDoor_.ApplyEditorTransform();
            }
        } else if (layout.id == "timed_switch") {
            if (applyObjectTransform(pastSelfTutorialTimedSwitch_.GetEditorObject(), layout)) {
                pastSelfTutorialTimedSwitch_.ApplyEditorTransform();
            }
        } else if (layout.id == "timed_door") {
            if (applyObjectTransform(pastSelfTutorialTimedDoor_.GetEditorObject(), layout)) {
                pastSelfTutorialTimedDoor_.ApplyEditorTransform();
            }
        } else if (layout.id == "toggle_switch") {
            if (applyObjectTransform(pastSelfTutorialToggleSwitch_.GetEditorObject(), layout)) {
                pastSelfTutorialToggleSwitch_.ApplyEditorTransform();
            }
        } else if (layout.id == "toggle_gate") {
            if (applyObjectTransform(pastSelfTutorialToggleGate_.GetEditorObject(), layout)) {
                pastSelfTutorialToggleGate_.ApplyEditorTransform();
            }
        } else if (layout.id == "toggle_elevator" && layout.hasUpperTranslate) {
            pastSelfTutorialToggleElevator_.RestoreEditorTransform(layout.scale, layout.rotate, layout.translate, layout.upperTranslate);
        } else if (layout.id == "weight_switch") {
            if (applyObjectTransform(pastSelfTutorialWeightSwitch_.GetEditorObject(), layout)) {
                pastSelfTutorialWeightSwitch_.ApplyEditorTransform();
            }
        } else if (layout.id == "goal_bridge") {
            if (applyObjectTransform(pastSelfTutorialGoalBridge_.GetEditorObject(), layout)) {
                pastSelfTutorialGoalBridge_.ApplyEditorTransform();
            }
        } else if (layout.id == "one_way_gate") {
            if (applyObjectTransform(pastSelfTutorialOneWayGate_.GetEditorObject(), layout)) {
                pastSelfTutorialOneWayGate_.ApplyEditorTransform();
            }
        }
    }
}

/// <summary>
/// 現在の攻略状況を収めるゲーム用カメラ位置へ戻す。
/// </summary>
void PlayScene::ResetPastSelfTutorialCameraFrame()
{
    const std::vector<PlayerState> visibleCloneStates = pastSelfCloneManager_.GetVisibleStates(); // カメラ範囲へ含める可視分身状態一覧
    const PastSelfTutorialCameraFrame cameraFrame = CalculatePastSelfTutorialCameraFrame(
        player_.GetState(), visibleCloneStates, GetPastSelfTutorialRouteCameraTarget(player_.GetState())); // 現在の攻略対象を収めるカメラ範囲
    pastSelfTutorialCameraFocus_ = cameraFrame.focus;
    pastSelfTutorialCameraDistance_ = cameraFrame.distance;
    ConfigurePastSelfTutorialCamera(ctx_.camera, pastSelfTutorialCameraFocus_, pastSelfTutorialCameraDistance_);
}

/// <summary>
/// 選択中ルート用の新しいステージブロックを追加する。
/// </summary>
size_t PlayScene::CreatePastSelfTutorialStageBlock()
{
    uint32_t routeMask = kPastSelfTutorialFinalRouteMask; // 新規ブロックを使用するルート
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        routeMask |= kPastSelfTutorialOneCloneRouteMask;
    } else if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        routeMask |= kPastSelfTutorialTwoCloneRouteMask;
    }
    const std::string blockName = "Stage Block " + std::to_string(pastSelfTutorialStageBlocks_.size()); // 新規ブロック名
    return AppendPastSelfTutorialStageBlock(blockName, { 2.0f, 0.25f, 4.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.75f, 0.82f, 0.9f, 1.0f }, true, false, false, routeMask);
}

/// <summary>
/// 指定したステージブロックを複製する。
/// </summary>
size_t PlayScene::DuplicatePastSelfTutorialStageBlock(size_t blockIndex)
{
    if (blockIndex >= pastSelfTutorialStageBlocks_.size() || !pastSelfTutorialStageBlocks_[blockIndex].object) {
        return pastSelfTutorialStageBlocks_.size();
    }
    PastSelfTutorialStageBlock& sourceBlock = pastSelfTutorialStageBlocks_[blockIndex]; // 複製元のステージブロック
    const Math::Vector3 duplicatedTranslate = sourceBlock.object->GetTranslate() + Math::Vector3 { 0.5f, 0.5f, 0.0f }; // 元と重ならない複製座標
    return AppendPastSelfTutorialStageBlock(sourceBlock.name + " Copy", sourceBlock.object->GetScale(), sourceBlock.object->GetRotate(), duplicatedTranslate, sourceBlock.baseColor, sourceBlock.collider.enabled, false, false, sourceBlock.routeMask);
}

/// <summary>
/// 指定したステージブロックを削除する。
/// </summary>
bool PlayScene::DeletePastSelfTutorialStageBlock(size_t blockIndex)
{
    if (blockIndex >= pastSelfTutorialStageBlocks_.size() || pastSelfTutorialStageBlocks_[blockIndex].goalMarker) {
        return false;
    }
    pastSelfTutorialStageBlocks_.erase(pastSelfTutorialStageBlocks_.begin() + static_cast<std::ptrdiff_t>(blockIndex));
    return true;
}

/// <summary>
/// 分身チュートリアル用の分身ギミックを初期化する。
/// </summary>
void PlayScene::InitializePastSelfTutorialMechanics()
{
    BoxSwitchGimmickDesc switchDesc {}; // 分身専用スイッチの初期化情報
    switchDesc.objectId = IssueObjectId();
    switchDesc.modelFileName = kPastSelfTutorialModelFileName;
    switchDesc.scale = kPastSelfTutorialSwitchScale;
    switchDesc.translate = kPastSelfTutorialSwitchTranslate;
    switchDesc.volumeCenter = kPastSelfTutorialSwitchVolumeCenter;
    switchDesc.volumeHalfSize = kPastSelfTutorialSwitchVolumeHalfSize;
    switchDesc.inactiveColor = kPastSelfTutorialSwitchInactiveColor;
    switchDesc.activeColor = kPastSelfTutorialSwitchActiveColor;
    switchDesc.playerOnlyColor = kPastSelfTutorialSwitchPlayerOnlyColor;

    LinkedDoorGimmickDesc doorDesc {}; // スイッチ連動扉の初期化情報
    doorDesc.objectId = IssueObjectId();
    doorDesc.modelFileName = kPastSelfTutorialModelFileName;
    doorDesc.scale = kPastSelfTutorialDoorScale;
    doorDesc.translate = kPastSelfTutorialDoorTranslate;
    doorDesc.closedColor = kPastSelfTutorialDoorClosedColor;
    doorDesc.openColor = kPastSelfTutorialDoorOpenColor;


    TimedSwitchGimmickDesc timedSwitchDesc {}; // 時間差スイッチの初期化情報
    timedSwitchDesc.objectId = IssueObjectId();
    timedSwitchDesc.modelFileName = kPastSelfTutorialModelFileName;
    timedSwitchDesc.scale = kPastSelfTutorialTimedSwitchScale;
    timedSwitchDesc.translate = kPastSelfTutorialTimedSwitchTranslate;
    timedSwitchDesc.volumeCenter = kPastSelfTutorialTimedSwitchVolumeCenter;
    timedSwitchDesc.volumeHalfSize = kPastSelfTutorialTimedSwitchVolumeHalfSize;
    timedSwitchDesc.inactiveColor = kPastSelfTutorialTimedSwitchInactiveColor;
    timedSwitchDesc.activeColor = kPastSelfTutorialTimedSwitchActiveColor;
    timedSwitchDesc.triggerColor = kPastSelfTutorialTimedSwitchTriggerColor;
    timedSwitchDesc.holdSeconds = kPastSelfTutorialTimedSwitchHoldSeconds;

    LinkedDoorGimmickDesc timedDoorDesc {}; // 時間差扉の初期化情報
    timedDoorDesc.objectId = IssueObjectId();
    timedDoorDesc.modelFileName = kPastSelfTutorialModelFileName;
    timedDoorDesc.scale = kPastSelfTutorialTimedDoorScale;
    timedDoorDesc.translate = kPastSelfTutorialTimedDoorTranslate;
    timedDoorDesc.closedColor = kPastSelfTutorialTimedDoorClosedColor;
    timedDoorDesc.openColor = kPastSelfTutorialTimedDoorOpenColor;

    ToggleSwitchGimmickDesc toggleSwitchDesc {}; // 1体用ルートのトグルスイッチ初期化情報
    toggleSwitchDesc.objectId = IssueObjectId();
    toggleSwitchDesc.modelFileName = kPastSelfTutorialModelFileName;
    toggleSwitchDesc.scale = kPastSelfTutorialToggleSwitchScale;
    toggleSwitchDesc.translate = kPastSelfTutorialToggleSwitchTranslate;
    toggleSwitchDesc.volumeCenter = kPastSelfTutorialToggleSwitchVolumeCenter;
    toggleSwitchDesc.volumeHalfSize = kPastSelfTutorialToggleSwitchVolumeHalfSize;
    toggleSwitchDesc.inactiveColor = kPastSelfTutorialToggleSwitchInactiveColor;
    toggleSwitchDesc.activeColor = kPastSelfTutorialToggleSwitchActiveColor;
    toggleSwitchDesc.pressedColor = kPastSelfTutorialToggleSwitchPressedColor;

    LinkedDoorGimmickDesc toggleGateDesc {}; // トグルスイッチに連動する1体用ルートゲートの初期化情報
    toggleGateDesc.objectId = IssueObjectId();
    toggleGateDesc.modelFileName = kPastSelfTutorialModelFileName;
    toggleGateDesc.scale = kPastSelfTutorialToggleGateScale;
    toggleGateDesc.translate = kPastSelfTutorialToggleGateTranslate;
    toggleGateDesc.closedColor = kPastSelfTutorialToggleGateClosedColor;
    toggleGateDesc.openColor = kPastSelfTutorialToggleGateOpenColor;

    MovingPlatformGimmickDesc toggleElevatorDesc {}; // トグルスイッチに連動する昇降足場の初期化情報
    toggleElevatorDesc.objectId = IssueObjectId();
    toggleElevatorDesc.modelFileName = kPastSelfTutorialModelFileName;
    toggleElevatorDesc.scale = kPastSelfTutorialToggleElevatorScale;
    toggleElevatorDesc.lowerTranslate = kPastSelfTutorialToggleElevatorLowerTranslate;
    toggleElevatorDesc.upperTranslate = kPastSelfTutorialToggleElevatorUpperTranslate;
    toggleElevatorDesc.inactiveColor = kPastSelfTutorialToggleElevatorInactiveColor;
    toggleElevatorDesc.activeColor = kPastSelfTutorialToggleElevatorActiveColor;
    toggleElevatorDesc.moveSpeed = kPastSelfTutorialToggleElevatorMoveSpeed;
    toggleElevatorDesc.upperWaitSeconds = kPastSelfTutorialToggleElevatorUpperWaitSeconds;
    toggleElevatorDesc.lowerWaitSeconds = kPastSelfTutorialToggleElevatorLowerWaitSeconds;

    WeightSwitchGimmickDesc weightSwitchDesc {}; // 重さスイッチの初期化情報
    weightSwitchDesc.objectId = IssueObjectId();
    weightSwitchDesc.modelFileName = kPastSelfTutorialModelFileName;
    weightSwitchDesc.scale = kPastSelfTutorialWeightSwitchScale;
    weightSwitchDesc.translate = kPastSelfTutorialWeightSwitchTranslate;
    weightSwitchDesc.volumeCenter = kPastSelfTutorialWeightSwitchVolumeCenter;
    weightSwitchDesc.volumeHalfSize = kPastSelfTutorialWeightSwitchVolumeHalfSize;
    weightSwitchDesc.inactiveColor = kPastSelfTutorialWeightSwitchInactiveColor;
    weightSwitchDesc.partialColor = kPastSelfTutorialWeightSwitchPartialColor;
    weightSwitchDesc.activeColor = kPastSelfTutorialWeightSwitchActiveColor;

    LinkedBridgeGimmickDesc goalBridgeDesc {}; // 重さスイッチに連動するゴール前の橋の初期化情報
    goalBridgeDesc.objectId = IssueObjectId();
    goalBridgeDesc.modelFileName = kPastSelfTutorialModelFileName;
    goalBridgeDesc.scale = kPastSelfTutorialGoalBridgeScale;
    goalBridgeDesc.translate = kPastSelfTutorialGoalBridgeTranslate;
    goalBridgeDesc.retractedColor = kPastSelfTutorialGoalBridgeRetractedColor;
    goalBridgeDesc.deployedColor = kPastSelfTutorialGoalBridgeDeployedColor;

    OneWayGateGimmickDesc oneWayGateDesc {}; // 一方通行ゲートの初期化情報
    oneWayGateDesc.objectId = IssueObjectId();
    oneWayGateDesc.modelFileName = kPastSelfTutorialModelFileName;
    oneWayGateDesc.scale = kPastSelfTutorialOneWayGateScale;
    oneWayGateDesc.translate = kPastSelfTutorialOneWayGateTranslate;
    oneWayGateDesc.passableColor = kPastSelfTutorialOneWayGatePassableColor;
    oneWayGateDesc.blockingColor = kPastSelfTutorialOneWayGateBlockingColor;
    oneWayGateDesc.allowedDirectionX = 1.0f;

    pastSelfTutorialSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, switchDesc);
    pastSelfTutorialDoor_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, doorDesc);
    pastSelfTutorialTimedSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, timedSwitchDesc);
    pastSelfTutorialTimedDoor_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, timedDoorDesc);
    pastSelfTutorialToggleSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, toggleSwitchDesc);
    pastSelfTutorialToggleGate_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, toggleGateDesc);
    pastSelfTutorialToggleElevator_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, toggleElevatorDesc);
    pastSelfTutorialWeightSwitch_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, weightSwitchDesc);
    pastSelfTutorialGoalBridge_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, goalBridgeDesc);
    pastSelfTutorialOneWayGate_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, oneWayGateDesc);
    pastSelfTutorialCloneStartMarkerObject_ = CreatePastSelfTutorialBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPastSelfTutorialCloneStartMarkerScale, kPastSelfTutorialStartTranslate, kPastSelfTutorialCloneStartMarkerColor);
    pastSelfTutorialCloneEndMarkerObject_ = CreatePastSelfTutorialBlockObject(ctx_.object3dCommon, ctx_.imguiManager, IssueObjectId(), kPastSelfTutorialCloneEndMarkerScale, kPastSelfTutorialStartTranslate, kPastSelfTutorialCloneEndMarkerColor);
    pastSelfTutorialState_ = {};
    pastSelfTutorialState_.ResetPuzzle();
    ConfigurePastSelfTutorialGoal();
    pastSelfTutorialDefaultGimmickLayouts_ = CollectPastSelfTutorialGimmickLayouts();
}

/// <summary>
/// 分身チュートリアル用の分身ギミックを更新する。
/// </summary>
void PlayScene::UpdatePastSelfTutorialMechanics(float deltaTime)
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
    pastSelfTutorialSwitch_.Update(playerState, cloneStateView);
    pastSelfTutorialState_.gimmicks.playerOnSwitch = pastSelfTutorialSwitch_.IsPlayerOnSwitch();
    pastSelfTutorialState_.gimmicks.cloneOnSwitch = pastSelfTutorialSwitch_.IsCloneOnSwitch();
    pastSelfTutorialState_.gimmicks.switchActive = pastSelfTutorialSwitch_.IsActive();
    if (playbackCloneVisible && pastSelfTutorialState_.gimmicks.switchActive) {
        pastSelfTutorialState_.gimmicks.doorUnlockedByClone = true;
    }
    pastSelfTutorialDoor_.Update(pastSelfTutorialState_.gimmicks.doorUnlockedByClone);
    pastSelfTutorialState_.gimmicks.doorOpen = pastSelfTutorialDoor_.IsOpen();

    pastSelfTutorialTimedSwitch_.Update(deltaTime, timedSwitchInputView);
    pastSelfTutorialState_.gimmicks.timedSwitchActive = pastSelfTutorialTimedSwitch_.IsActive();
    pastSelfTutorialState_.gimmicks.timedSwitchCloneOn = pastSelfTutorialTimedSwitch_.IsCloneOnSwitch();
    pastSelfTutorialWeightSwitch_.Update(playerState, cloneStateView);
    pastSelfTutorialState_.gimmicks.weightSwitchActive = pastSelfTutorialWeightSwitch_.IsActive();
    pastSelfTutorialState_.gimmicks.weightPlayerOn = pastSelfTutorialWeightSwitch_.IsPlayerOnSwitch();
    pastSelfTutorialState_.gimmicks.weightCloneOn = pastSelfTutorialWeightSwitch_.IsCloneOnSwitch();
    const bool dualCloneSwitchInputActive = pastSelfTutorialState_.gimmicks.switchActive && pastSelfTutorialState_.gimmicks.timedSwitchActive; // 緑と青のスイッチが同時に起動しているか
    pastSelfTutorialTimedDoor_.Update(dualCloneSwitchInputActive || pastSelfTutorialState_.gimmicks.weightSwitchActive);
    pastSelfTutorialState_.gimmicks.timedDoorOpen = pastSelfTutorialTimedDoor_.IsOpen();
    pastSelfTutorialToggleSwitch_.Update(cloneStateView);
    pastSelfTutorialState_.gimmicks.toggleSwitchActive = pastSelfTutorialToggleSwitch_.IsActive();
    pastSelfTutorialState_.gimmicks.toggleSwitchCloneOn = pastSelfTutorialToggleSwitch_.IsCloneOnSwitch();
    pastSelfTutorialToggleGate_.Update(pastSelfTutorialState_.gimmicks.toggleSwitchActive);
    pastSelfTutorialState_.gimmicks.toggleGateOpen = pastSelfTutorialToggleGate_.IsOpen();
    pastSelfTutorialToggleElevator_.Update(deltaTime, pastSelfTutorialState_.gimmicks.toggleSwitchActive);
    pastSelfTutorialState_.gimmicks.toggleElevatorActive = pastSelfTutorialToggleElevator_.IsActive();
    const bool oneCloneTutorialEligible = pastSelfTutorialState_.progress.recordTakeCount == 1 &&
        pastSelfCloneManager_.GetCloneCount() == 1 && pastSelfTutorialState_.progress.recordStopped &&
        pastSelfTutorialState_.progress.prepareUsed && pastSelfTutorialState_.progress.replayStarted; // 1回の記録と1体の分身で再生準備まで行ったか
    if (oneCloneTutorialEligible && pastSelfTutorialState_.gimmicks.toggleSwitchCloneOn && pastSelfTutorialState_.gimmicks.toggleSwitchActive &&
        !pastSelfTutorialState_.replay.oneCloneToggleActivated) {
        pastSelfTutorialState_.replay.oneCloneToggleActivated = true;
        RegisterPastSelfTutorialCheckCompleted("Clone activated the orange toggle");
    }
    pastSelfTutorialOneWayGate_.Update(player_.GetState());
    pastSelfTutorialState_.gimmicks.oneWayGateBlocking = pastSelfTutorialOneWayGate_.IsBlocking();

    if (playbackCloneVisible && pastSelfTutorialState_.gimmicks.cloneOnSwitch && !pastSelfTutorialState_.progress.doorOpenedByClone) {
        pastSelfTutorialState_.progress.doorOpenedByClone = true;
        RegisterPastSelfTutorialCheckCompleted("Clone opened green door");
    }
    if (pastSelfTutorialState_.gimmicks.timedDoorOpen && dualCloneSwitchInputActive && !pastSelfTutorialState_.progress.timedDoorOpened) {
        pastSelfTutorialState_.progress.timedDoorOpened = true;
        RegisterPastSelfTutorialCheckCompleted("Green and blue switches opened blue door");
    }
    if (!pastSelfRecorder_.IsRecording() && cloneStates.size() >= 2 && pastSelfTutorialState_.gimmicks.switchActive && pastSelfTutorialState_.gimmicks.timedSwitchCloneOn) {
        pastSelfTutorialState_.progress.dualCloneSwitchesActivated = true;
    }
    const bool twoCloneTutorialEligible = !pastSelfRecorder_.IsRecording() &&
        pastSelfTutorialState_.progress.recordTakeCount == 2 && pastSelfCloneManager_.GetCloneCount() == 2 &&
        pastSelfTutorialState_.progress.recordStopped && pastSelfTutorialState_.replay.twoCloneReplayPrepared; // 2回の記録と2体の分身で再生準備したか
    if (twoCloneTutorialEligible && cloneStates.size() == 2 && pastSelfTutorialState_.gimmicks.switchActive &&
        pastSelfTutorialState_.gimmicks.timedSwitchCloneOn && !pastSelfTutorialState_.replay.twoCloneSwitchesActivated) {
        pastSelfTutorialState_.replay.twoCloneSwitchesActivated = true;
        RegisterPastSelfTutorialCheckCompleted("Two clones activated green and blue switches");
    }
    if (pastSelfTutorialState_.gimmicks.weightSwitchActive && !pastSelfTutorialState_.progress.weightSwitchActivated) {
        pastSelfTutorialState_.progress.weightSwitchActivated = true;
        RegisterPastSelfTutorialCheckCompleted("Player and clone activated yellow switch");
    }
    if (pastSelfTutorialState_.gimmicks.weightSwitchActive) {
        pastSelfTutorialState_.progress.goalBridgeUnlocked = true;
    }
    pastSelfTutorialGoalBridge_.Update(pastSelfTutorialState_.progress.goalBridgeUnlocked);
    pastSelfTutorialState_.gimmicks.goalBridgeDeployed = pastSelfTutorialGoalBridge_.IsDeployed();
    if (pastSelfTutorialState_.gimmicks.oneWayGateBlocking && !pastSelfTutorialState_.progress.oneWayGateUsed) {
        pastSelfTutorialState_.progress.oneWayGateUsed = true;
        RegisterPastSelfTutorialCheckCompleted("Purple gate blocked the return path");
    }
}

/// <summary>
/// 分身チュートリアル用状態を初期状態へ戻す。
/// </summary>
void PlayScene::ResetPastSelfTutorialState()
{
    player_.Reset();
    pastSelfRecorder_.Clear();
    pastSelfCloneManager_.Clear();
    ResetPastSelfTutorialMechanics();
    pastSelfTutorialState_.ResetPuzzle();
    RefreshPastSelfTutorialAfterReset();
}

/// <summary>
/// 記録済み分身を残したまま再生開始用の状態へ戻す。
/// </summary>
void PlayScene::ResetPastSelfTutorialReplayState(bool registerPrepareAction)
{
    player_.Reset();
    pastSelfCloneManager_.StopAll();
    ResetPastSelfTutorialMechanics();
    pastSelfTutorialState_.PrepareReplay(registerPrepareAction, pastSelfCloneManager_.GetCloneCount(), kPrepareFeedbackDuration);
    RefreshPastSelfTutorialAfterReset();
}

/// <summary>
/// 通常リセットと再生準備で共通するギミックを初期状態へ戻す。
/// </summary>
void PlayScene::ResetPastSelfTutorialMechanics()
{
    pastSelfTutorialGoal_.Reset();
    pastSelfTutorialSwitch_.Reset();
    pastSelfTutorialDoor_.Reset();
    pastSelfTutorialTimedSwitch_.Reset();
    pastSelfTutorialTimedDoor_.Reset();
    pastSelfTutorialToggleSwitch_.Reset();
    pastSelfTutorialToggleGate_.Reset();
    pastSelfTutorialToggleElevator_.Reset();
    pastSelfTutorialWeightSwitch_.Reset();
    pastSelfTutorialGoalBridge_.Reset();
    pastSelfTutorialOneWayGate_.Reset();
}

/// <summary>
/// リセット後のプレイヤー色・カメラとギミック表示を更新する。
/// </summary>
void PlayScene::RefreshPastSelfTutorialAfterReset()
{
    player_.SetMaterialColor(kPastSelfTutorialNormalPlayerColor);
    const PastSelfTutorialCameraFrame cameraFrame = CalculatePastSelfTutorialCameraFrame(
        player_.GetState(), {}, GetPastSelfTutorialRouteCameraTarget(player_.GetState())); // 開始地点と攻略対象を収める範囲
    pastSelfTutorialCameraFocus_ = cameraFrame.focus;
    pastSelfTutorialCameraDistance_ = cameraFrame.distance;
    UpdatePastSelfTutorialMechanics(0.0f);
    ApplyPastSelfTutorialGoalVisual();
}

/// <summary>
/// 最後に保存した分身を削除して再生準備状態へ戻す。
/// </summary>
void PlayScene::UndoLastPastSelfTutorialClone()
{
    if (pastSelfRecorder_.IsRecording() || !pastSelfCloneManager_.RemoveLastClone()) {
        return;
    }

    pastSelfTutorialState_.progress.lastRecordDuration = pastSelfCloneManager_.GetLastCloneDuration();
    ResetPastSelfTutorialReplayState(false);
}

/// <summary>
/// 現在のステージルールで新しい分身記録を開始できるか判定する。
/// </summary>
bool PlayScene::CanStartPastSelfTutorialRecording() const
{
    return !IsPastSelfTutorialSelectedRouteComplete() && !pastSelfRecorder_.IsRecording() &&
        pastSelfCloneManager_.GetCloneCount() < pastSelfTutorialStageRules_.maxStoredClones;
}

/// <summary>
/// 新しい分身用のプレイヤー記録を開始する。
/// </summary>
void PlayScene::StartPastSelfTutorialRecording()
{
    if (!CanStartPastSelfTutorialRecording()) {
        return;
    }

    if (pastSelfCloneManager_.StartAll()) {
        pastSelfTutorialState_.progress.replayStarted = true;
    }
    pastSelfRecorder_.Start();
    ++pastSelfTutorialState_.progress.recordTakeCount;
    pastSelfTutorialState_.progress.recordStarted = true;
    pastSelfTutorialState_.replay.recordingPendingCommit = true;
    pastSelfTutorialState_.progress.lastRecordDuration = 0.0f;
}

/// <summary>
/// 現在の記録を停止し、新しい分身として保存する。
/// </summary>
void PlayScene::FinishPastSelfTutorialRecording()
{
    if (!pastSelfTutorialState_.replay.recordingPendingCommit) {
        return;
    }

    pastSelfRecorder_.Stop();
    pastSelfTutorialState_.progress.lastRecordDuration = pastSelfRecorder_.GetDuration();
    const bool cloneAdded = pastSelfCloneManager_.AddClone(pastSelfRecorder_.GetFrames()); // 有効な記録から分身を保存できたか
    pastSelfTutorialState_.progress.recordStopped = pastSelfTutorialState_.progress.recordStopped || cloneAdded;
    pastSelfTutorialState_.replay.recordingPendingCommit = false;
}

/// <summary>
/// 分身チュートリアル用の分身ギミック表示を更新する。
/// </summary>
void PlayScene::UpdatePastSelfTutorialMechanicObjects(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        pastSelfTutorialToggleSwitch_.UpdateObject(viewMatrix, projectionMatrix);
        pastSelfTutorialToggleGate_.UpdateObject(viewMatrix, projectionMatrix);
        pastSelfTutorialToggleElevator_.UpdateObject(viewMatrix, projectionMatrix);
        return;
    }
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        pastSelfTutorialSwitch_.UpdateObject(viewMatrix, projectionMatrix);
        pastSelfTutorialDoor_.UpdateObject(viewMatrix, projectionMatrix);
        pastSelfTutorialTimedSwitch_.UpdateObject(viewMatrix, projectionMatrix);
        pastSelfTutorialTimedDoor_.UpdateObject(viewMatrix, projectionMatrix);
        return;
    }
    pastSelfTutorialSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialDoor_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialTimedSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialTimedDoor_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialToggleSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialToggleGate_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialToggleElevator_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialWeightSwitch_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialGoalBridge_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfTutorialOneWayGate_.UpdateObject(viewMatrix, projectionMatrix);
}

/// <summary>
/// 分身記録の開始・終了地点マーカーを更新する。
/// </summary>
void PlayScene::UpdatePastSelfTutorialCloneRecordMarkers(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    if (!pastSelfTutorialCloneStartMarkerObject_ || !pastSelfTutorialCloneEndMarkerObject_) {
        return;
    }

    const std::vector<PastSelfFrame>& frames = pastSelfRecorder_.GetFrames(); // 記録済みの分身フレーム
    if (frames.empty() || pastSelfRecorder_.IsRecording()) {
        return;
    }

    const PlayerState& startState = frames.front().state; // 分身の開始位置として使う最初の状態
    pastSelfTutorialCloneStartMarkerObject_->SetScale(kPastSelfTutorialCloneStartMarkerScale);
    pastSelfTutorialCloneStartMarkerObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    pastSelfTutorialCloneStartMarkerObject_->SetTranslate(startState.transform.translate);
    pastSelfTutorialCloneStartMarkerObject_->Update(viewMatrix, projectionMatrix);

    const PlayerState& endState = frames.back().state; // 分身の終了位置として使う最後の状態
    pastSelfTutorialCloneEndMarkerObject_->SetScale(kPastSelfTutorialCloneEndMarkerScale);
    pastSelfTutorialCloneEndMarkerObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    pastSelfTutorialCloneEndMarkerObject_->SetTranslate(endState.transform.translate);
    pastSelfTutorialCloneEndMarkerObject_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// 分身記録の開始・終了地点マーカーを描画する。
/// </summary>
void PlayScene::DrawPastSelfTutorialCloneRecordMarkers()
{
    if (!pastSelfTutorialCloneStartMarkerObject_ || !pastSelfTutorialCloneEndMarkerObject_) {
        return;
    }

    const std::vector<PastSelfFrame>& frames = pastSelfRecorder_.GetFrames(); // 表示判定に使う記録済みフレーム
    if (frames.empty() || pastSelfRecorder_.IsRecording()) {
        return;
    }

    DrawObjectWithAlphaBlend(pastSelfTutorialCloneStartMarkerObject_.get());
    DrawObjectWithAlphaBlend(pastSelfTutorialCloneEndMarkerObject_.get());
}

/// <summary>
/// 分身チュートリアル用の分身ギミックを描画する。
/// </summary>
void PlayScene::DrawPastSelfTutorialMechanics()
{
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        pastSelfTutorialToggleSwitch_.Draw();
        pastSelfTutorialToggleGate_.Draw();
        pastSelfTutorialToggleElevator_.Draw();
        return;
    }
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        pastSelfTutorialSwitch_.Draw();
        pastSelfTutorialDoor_.Draw();
        pastSelfTutorialTimedSwitch_.Draw();
        pastSelfTutorialTimedDoor_.Draw();
        return;
    }
    pastSelfTutorialSwitch_.Draw();
    pastSelfTutorialDoor_.Draw();
    pastSelfTutorialTimedSwitch_.Draw();
    pastSelfTutorialTimedDoor_.Draw();
    pastSelfTutorialToggleSwitch_.Draw();
    pastSelfTutorialToggleGate_.Draw();
    pastSelfTutorialToggleElevator_.Draw();
    pastSelfTutorialWeightSwitch_.Draw();
    pastSelfTutorialGoalBridge_.Draw();
    pastSelfTutorialOneWayGate_.Draw();
}

/// <summary>
/// 分身チュートリアル用状態を更新する。
/// </summary>
void PlayScene::UpdatePastSelfTutorial(float deltaTime)
{
    pastSelfTutorialState_.replay.prepareFeedbackSeconds = (std::max)(pastSelfTutorialState_.replay.prepareFeedbackSeconds - deltaTime, 0.0f);
    pastSelfTutorialState_.progress.recentCheckSeconds = (std::max)(pastSelfTutorialState_.progress.recentCheckSeconds - deltaTime, 0.0f);
    const bool blockInputByImGui = ShouldBlockPlayerInput(); // ImGui操作でゲーム入力を止めるか
    InputManager* inputManager = InputManager::GetInstance(); // 分身チュートリアル用入力を取得する管理クラス
    if (!blockInputByImGui && inputManager && inputManager->IsKeyJustPressed(kPastSelfTutorialResetKey)) {
        ResetPastSelfTutorialState();
    }
    const bool selectedRouteComplete = IsPastSelfTutorialSelectedRouteComplete(); // 選択中ルートがクリア済みか
    const bool canPrepareReplay = !selectedRouteComplete && !blockInputByImGui && inputManager &&
        !pastSelfRecorder_.IsRecording() && pastSelfCloneManager_.GetCloneCount() > 0; // Prepare入力を受け付けられるか
    if (canPrepareReplay && inputManager->IsKeyJustPressed(kReplayPrepareKey)) {
        ResetPastSelfTutorialReplayState();
    }
    const bool canUndoLastClone = !selectedRouteComplete && !blockInputByImGui && inputManager &&
        !pastSelfRecorder_.IsRecording() && pastSelfCloneManager_.GetCloneCount() > 0; // 分身削除入力を受け付けられるか
    if (canUndoLastClone && inputManager->IsKeyJustPressed(kCloneUndoKey)) {
        UndoLastPastSelfTutorialClone();
    }

    const bool canAcceptInput = !selectedRouteComplete && !blockInputByImGui; // プレイヤーと分身操作の入力を受け取れるか
    if (canAcceptInput && inputManager) {
        if (inputManager->IsKeyJustPressed(kRecordToggleKey)) {
            if (pastSelfRecorder_.IsRecording()) {
                FinishPastSelfTutorialRecording();
            } else {
                StartPastSelfTutorialRecording();
            }
        }
        if (inputManager->IsKeyJustPressed(kClonePlayKey)) {
            if (pastSelfCloneManager_.StartAll()) {
                pastSelfTutorialState_.progress.replayStarted = true;
            }
        }
        if (inputManager->IsKeyJustPressed(kCloneStopKey)) {
            pastSelfCloneManager_.StopAll();
        }
    }

    if (!selectedRouteComplete) {
        pastSelfTutorialState_.progress.elapsedTime += deltaTime;
        pastSelfCloneManager_.Update(deltaTime, BuildCloneStandablePlatforms(player_));
        UpdatePastSelfTutorialMechanics(deltaTime);
        const SolidCollider previousElevatorCollider = pastSelfTutorialToggleElevator_.GetPreviousSolidCollider(); // 更新前の昇降足場コライダー
        const SolidCollider currentElevatorCollider = pastSelfTutorialToggleElevator_.GetSolidCollider(); // 更新後の昇降足場コライダー
        std::vector<SolidCollider> solidColliders; // プレイヤーが全面衝突する地形と扉
        AppendPastSelfTutorialSolidColliders(&solidColliders);
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
        if (pastSelfTutorialState_.replay.oneCloneToggleActivated && playerStandingOnToggleElevator &&
            !pastSelfTutorialState_.replay.oneCloneElevatorRidden) {
            pastSelfTutorialState_.replay.oneCloneElevatorRidden = true;
            RegisterPastSelfTutorialCheckCompleted("Player rode the orange moving lift");
        }
        constexpr float kElevatorEndpointTolerance = 0.05f; // 上端到達判定に許容する位置誤差
        const bool elevatorAtUpperEndpoint = PastSelfTutorialLayoutUtility::IsAtUpperEndpoint(
            pastSelfTutorialToggleElevator_.GetCurrentTranslate(), pastSelfTutorialToggleElevator_.GetEditorUpperTranslate(),
            kElevatorEndpointTolerance); // 編集後の昇降範囲の上端へ到達しているか
        if (pastSelfTutorialState_.replay.oneCloneElevatorRidden && playerStandingOnToggleElevator && elevatorAtUpperEndpoint &&
            !pastSelfTutorialState_.replay.oneCloneBasicsComplete) {
            pastSelfTutorialState_.replay.oneCloneBasicsComplete = true;
            RegisterPastSelfTutorialCheckCompleted("One-clone tutorial route complete");
        }
        const Math::Vector3 playerHalfSize = CalculatePlayerStateHalfSize(player_.GetState()); // 青扉通過判定に使うプレイヤー半サイズ
        const SolidCollider timedDoorCollider = pastSelfTutorialTimedDoor_.GetSolidCollider(); // 開閉によらず現在の青扉の判定範囲を取得する
        const bool playerPassedTimedDoor = PastSelfTutorialLayoutUtility::HasPassedDoor(
            player_.GetState().transform.translate, playerHalfSize, timedDoorCollider.center, timedDoorCollider.halfSize); // プレイヤー全体が編集後の青扉の右側へ抜けたか
        if (pastSelfTutorialState_.replay.twoCloneSwitchesActivated && pastSelfTutorialState_.progress.timedDoorOpened &&
            playerPassedTimedDoor && !pastSelfTutorialState_.replay.twoCloneCooperationComplete) {
            pastSelfTutorialState_.replay.twoCloneCooperationComplete = true;
            RegisterPastSelfTutorialCheckCompleted("Two-clone tutorial route complete");
        }
        const SolidCollider doorCollider = pastSelfTutorialDoor_.GetSolidCollider(); // 閉じている扉の衝突判定
        if (doorCollider.enabled && IsPlayerStateTouchingSolidCollider(player_.GetState(), doorCollider) && !pastSelfTutorialState_.progress.doorBlockedBeforeClone) {
            pastSelfTutorialState_.progress.doorBlockedBeforeClone = true;
            RegisterPastSelfTutorialCheckCompleted("Closed green door blocked the player");
        }
        if (IsPlayerStandingOnClonePlatform(player_.GetState(), standablePlatforms) && !pastSelfTutorialState_.progress.clonePlatformUsed) {
            pastSelfTutorialState_.progress.clonePlatformUsed = true;
            RegisterPastSelfTutorialCheckCompleted("Player used a clone as a platform");
        }
        if (player_.GetState().transform.translate.y < kPastSelfTutorialFallResetY) {
            ResetPastSelfTutorialState();
        }
        const bool wasRecording = pastSelfRecorder_.IsRecording(); // 記録更新前に記録中だったか
        pastSelfRecorder_.Update(deltaTime, player_.GetState());
        if (wasRecording && !pastSelfRecorder_.IsRecording()) {
            FinishPastSelfTutorialRecording();
        }
        if (!pastSelfRecorder_.IsRecording() && pastSelfRecorder_.GetFrames().size() >= 2) {
            pastSelfTutorialState_.progress.lastRecordDuration = pastSelfRecorder_.GetDuration();
        }
        const Math::Vector4 playerColor = pastSelfRecorder_.IsRecording() ? kPastSelfTutorialRecordingPlayerColor : kPastSelfTutorialNormalPlayerColor; // 記録状態に応じたプレイヤー色
        player_.SetMaterialColor(playerColor);
        UpdatePastSelfTutorialMechanics(0.0f);
        UpdatePastSelfTutorialGoal();
        FinalizePastSelfTutorialSelectedRoute();
    } else {
        player_.SetMaterialColor(kPastSelfTutorialClearPlayerColor);
    }

    if (pastSelfTutorialAutoCameraFollow_) {
        const std::vector<PlayerState> visibleCloneStates = pastSelfCloneManager_.GetVisibleStates(); // カメラ範囲に含める可視分身状態一覧
        const PastSelfTutorialCameraFrame targetCameraFrame = CalculatePastSelfTutorialCameraFrame(
            player_.GetState(), visibleCloneStates, GetPastSelfTutorialRouteCameraTarget(player_.GetState())); // 現在の攻略対象を収める目標カメラ範囲
        pastSelfTutorialCameraFocus_.x = FollowPastSelfTutorialCameraValue(pastSelfTutorialCameraFocus_.x, targetCameraFrame.focus.x, deltaTime);
        pastSelfTutorialCameraFocus_.y = FollowPastSelfTutorialCameraValue(pastSelfTutorialCameraFocus_.y, targetCameraFrame.focus.y, deltaTime);
        pastSelfTutorialCameraFocus_.z = FollowPastSelfTutorialCameraValue(pastSelfTutorialCameraFocus_.z, targetCameraFrame.focus.z, deltaTime);
        pastSelfTutorialCameraDistance_ = FollowPastSelfTutorialCameraValue(pastSelfTutorialCameraDistance_, targetCameraFrame.distance, deltaTime);
        ConfigurePastSelfTutorialCamera(ctx_.camera, pastSelfTutorialCameraFocus_, pastSelfTutorialCameraDistance_);
    }

    if (!ctx_.camera) {
        return;
    }

    const Math::Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // プレイヤー更新に使用するビュー行列
    const Math::Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // プレイヤー更新に使用する射影行列
    UpdatePastSelfTutorialStage(viewMatrix, projectionMatrix);
    UpdatePastSelfTutorialMechanicObjects(viewMatrix, projectionMatrix);
    UpdatePastSelfTutorialCloneRecordMarkers(viewMatrix, projectionMatrix);
    player_.UpdateObject(viewMatrix, projectionMatrix);
    pastSelfCloneManager_.UpdateObjects(viewMatrix, projectionMatrix);
}

/// <summary>
/// 分身チュートリアル用ステージを更新する。
/// </summary>
void PlayScene::UpdatePastSelfTutorialStage(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    for (size_t blockIndex = 0; blockIndex < pastSelfTutorialStageBlocks_.size(); ++blockIndex) {
        if (!IsPastSelfTutorialStageBlockEnabled(blockIndex)) {
            continue;
        }
        PastSelfTutorialStageBlock& stageBlock = pastSelfTutorialStageBlocks_[blockIndex]; // 更新対象のチュートリアルステージブロック
        if (stageBlock.object) {
            if (stageBlock.oneCloneGoalPlatform) {
                const bool oneCloneUpperGoalComplete = pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics &&
                    pastSelfTutorialState_.replay.oneCloneBasicsComplete; // 1体用上段をクリア表示にするか
                const Math::Vector4 upperGoalColor = oneCloneUpperGoalComplete
                    ? kPastSelfTutorialGoalClearColor
                    : stageBlock.baseColor; // 1体用上段の攻略状態を示す表示色
                stageBlock.object->SetMaterialColor(upperGoalColor);
            }
            stageBlock.object->Update(viewMatrix, projectionMatrix);
        }
    }
}

/// <summary>
/// 選択中のチュートリアルルートで指定されたチュートリアルステージブロックを使用するか判定する。
/// </summary>
bool PlayScene::IsPastSelfTutorialStageBlockEnabled(size_t blockIndex) const
{
    if (blockIndex >= pastSelfTutorialStageBlocks_.size()) {
        return false;
    }
    const uint32_t routeMask = pastSelfTutorialStageBlocks_[blockIndex].routeMask; // ブロックに設定された対象ルート
    switch (pastSelfTutorialRoute_) {
    case PastSelfTutorialRoute::OneCloneBasics:
        return (routeMask & kPastSelfTutorialOneCloneRouteMask) != 0;
    case PastSelfTutorialRoute::TwoCloneCooperation:
        return (routeMask & kPastSelfTutorialTwoCloneRouteMask) != 0;
    case PastSelfTutorialRoute::FinalChallenge:
        return (routeMask & kPastSelfTutorialFinalRouteMask) != 0;
    }
    return false;
}

/// <summary>
/// 選択中ルートで使用するステージ編集対象一覧を構築する。
/// </summary>
void PlayScene::BuildPastSelfTutorialEditorObjects(std::vector<PastSelfTutorialEditorObject>* outObjects)
{
    if (!outObjects) {
        return;
    }

    outObjects->clear();
    outObjects->reserve(pastSelfTutorialStageBlocks_.size() + 10);
    std::vector<PastSelfTutorialEditorObject> stageBlockObjects; // ルート内の固定ブロック編集対象
    stageBlockObjects.reserve(pastSelfTutorialStageBlocks_.size());
    for (size_t blockIndex = 0; blockIndex < pastSelfTutorialStageBlocks_.size(); ++blockIndex) {
        if (!IsPastSelfTutorialStageBlockEnabled(blockIndex) || !pastSelfTutorialStageBlocks_[blockIndex].object) {
            continue;
        }
        const char* blockLabel = pastSelfTutorialStageBlocks_[blockIndex].name.empty()
            ? "Stage Block"
            : pastSelfTutorialStageBlocks_[blockIndex].name.c_str(); // 編集一覧へ表示するブロック名
        stageBlockObjects.push_back({ pastSelfTutorialStageBlocks_[blockIndex].object.get(), blockLabel, PastSelfTutorialEditorObjectType::StageBlock, blockIndex });
    }
    if (!stageBlockObjects.empty()) {
        outObjects->insert(outObjects->end(), stageBlockObjects.begin(), stageBlockObjects.end() - 1);
    }

    const auto appendGimmick = [outObjects](Object3d* object, const char* label, PastSelfTutorialEditorObjectType type) {
        if (object) {
            outObjects->push_back({ object, label, type, 0 });
        }
    }; // 実ステージで使用中のギミックを編集一覧へ追加する処理
    const auto appendLastStageBlock = [outObjects, &stageBlockObjects]() {
        if (!stageBlockObjects.empty()) {
            outObjects->push_back(stageBlockObjects.back());
        }
    }; // 末尾へ追加された固定ブロックを編集一覧の最後へ配置する処理

    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        appendGimmick(pastSelfTutorialToggleSwitch_.GetEditorObject(), "Toggle Switch", PastSelfTutorialEditorObjectType::ToggleSwitch);
        appendGimmick(pastSelfTutorialToggleGate_.GetEditorObject(), "Toggle Gate", PastSelfTutorialEditorObjectType::ToggleGate);
        appendGimmick(pastSelfTutorialToggleElevator_.GetEditorObject(), "Toggle Elevator", PastSelfTutorialEditorObjectType::ToggleElevator);
        appendLastStageBlock();
        return;
    }
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        appendGimmick(pastSelfTutorialSwitch_.GetEditorObject(), "Clone Switch", PastSelfTutorialEditorObjectType::BoxSwitch);
        appendGimmick(pastSelfTutorialDoor_.GetEditorObject(), "Linked Door", PastSelfTutorialEditorObjectType::Door);
        appendGimmick(pastSelfTutorialTimedSwitch_.GetEditorObject(), "Timed Switch", PastSelfTutorialEditorObjectType::TimedSwitch);
        appendGimmick(pastSelfTutorialTimedDoor_.GetEditorObject(), "Timed Door", PastSelfTutorialEditorObjectType::TimedDoor);
        appendLastStageBlock();
        return;
    }

    appendGimmick(pastSelfTutorialSwitch_.GetEditorObject(), "Clone Switch", PastSelfTutorialEditorObjectType::BoxSwitch);
    appendGimmick(pastSelfTutorialDoor_.GetEditorObject(), "Linked Door", PastSelfTutorialEditorObjectType::Door);
    appendGimmick(pastSelfTutorialTimedSwitch_.GetEditorObject(), "Timed Switch", PastSelfTutorialEditorObjectType::TimedSwitch);
    appendGimmick(pastSelfTutorialTimedDoor_.GetEditorObject(), "Timed Door", PastSelfTutorialEditorObjectType::TimedDoor);
    appendGimmick(pastSelfTutorialToggleSwitch_.GetEditorObject(), "Toggle Switch", PastSelfTutorialEditorObjectType::ToggleSwitch);
    appendGimmick(pastSelfTutorialToggleGate_.GetEditorObject(), "Toggle Gate", PastSelfTutorialEditorObjectType::ToggleGate);
    appendGimmick(pastSelfTutorialToggleElevator_.GetEditorObject(), "Toggle Elevator", PastSelfTutorialEditorObjectType::ToggleElevator);
    appendGimmick(pastSelfTutorialWeightSwitch_.GetEditorObject(), "Weight Switch", PastSelfTutorialEditorObjectType::WeightSwitch);
    appendGimmick(pastSelfTutorialGoalBridge_.GetEditorObject(), "Goal Bridge", PastSelfTutorialEditorObjectType::GoalBridge);
    appendGimmick(pastSelfTutorialOneWayGate_.GetEditorObject(), "One Way Gate", PastSelfTutorialEditorObjectType::OneWayGate);
    appendLastStageBlock();
}

/// <summary>
/// ステージ編集対象のTransform変更をゲーム判定へ反映する。
/// </summary>
void PlayScene::ApplyPastSelfTutorialEditorTransform(const PastSelfTutorialEditorObject& editorObject)
{
    if (!editorObject.object) {
        return;
    }

    switch (editorObject.type) {
    case PastSelfTutorialEditorObjectType::StageBlock:
        if (editorObject.stageBlockIndex < pastSelfTutorialStageBlocks_.size()) {
            PastSelfTutorialStageBlock& stageBlock = pastSelfTutorialStageBlocks_[editorObject.stageBlockIndex]; // 判定を同期するステージブロック
            const Math::Vector3 editedCenter = editorObject.object->GetTranslate(); // 編集後のステージブロック中心
            const Math::Vector3 editedHalfSize = CalculateStageBlockHalfSize(editorObject.object->GetScale()); // 編集後のステージブロック半サイズ
            stageBlock.collider.center = editedCenter;
            stageBlock.collider.halfSize = editedHalfSize;
            if (stageBlock.goalMarker) {
                const Math::Vector3 goalHalfSize = PastSelfTutorialLayoutUtility::CalculateGoalHalfSize(
                    editorObject.object->GetScale(), kPastSelfTutorialStageBlockDescs.back().scale,
                    kPastSelfTutorialGoalHalfSize); // 再読み込み時と同じ初期基準で計算したゴール判定半サイズ
                pastSelfTutorialGoal_.ApplyEditorTransform(editedCenter, goalHalfSize);
            }
        }
        break;
    case PastSelfTutorialEditorObjectType::BoxSwitch:
        pastSelfTutorialSwitch_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::Door:
        pastSelfTutorialDoor_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::TimedSwitch:
        pastSelfTutorialTimedSwitch_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::TimedDoor:
        pastSelfTutorialTimedDoor_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::ToggleSwitch:
        pastSelfTutorialToggleSwitch_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::ToggleGate:
        pastSelfTutorialToggleGate_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::ToggleElevator:
        pastSelfTutorialToggleElevator_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::WeightSwitch:
        pastSelfTutorialWeightSwitch_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::GoalBridge:
        pastSelfTutorialGoalBridge_.ApplyEditorTransform();
        break;
    case PastSelfTutorialEditorObjectType::OneWayGate:
        pastSelfTutorialOneWayGate_.ApplyEditorTransform();
        break;
    }
}

/// <summary>
/// 選択中のチュートリアルルートに応じたカメラ注視対象を取得する。
/// </summary>
Math::Vector3 PlayScene::GetPastSelfTutorialRouteCameraTarget(const PlayerState& playerState) const
{
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        return pastSelfTutorialToggleElevator_.GetEditorUpperTranslate();
    }
    const std::array<Object3d*, 4> targetObjects = { // 既存の攻略順で参照するギミック
        pastSelfTutorialDoor_.GetEditorObject(),
        pastSelfTutorialTimedDoor_.GetEditorObject(),
        pastSelfTutorialWeightSwitch_.GetEditorObject(),
        pastSelfTutorialOneWayGate_.GetEditorObject(),
    };
    std::array<Math::Vector3, 4> targetPositions {}; // 存在するギミックの現在配置
    size_t targetCount = 0; // 注視候補へ追加した配置数
    for (const Object3d* object : targetObjects) {
        if (object) {
            targetPositions[targetCount++] = object->GetTranslate();
        }
    }
    return PastSelfTutorialLayoutUtility::SelectCameraTarget(playerState.transform.translate.x,
        std::span<const Math::Vector3>(targetPositions.data(), targetCount), pastSelfTutorialGoal_.GetCenter());
}

/// <summary>
/// 分身チュートリアル用ステージを描画する。
/// </summary>
void PlayScene::DrawPastSelfTutorialStage()
{
    for (size_t blockIndex = 0; blockIndex < pastSelfTutorialStageBlocks_.size(); ++blockIndex) {
        if (!IsPastSelfTutorialStageBlockEnabled(blockIndex)) {
            continue;
        }
        PastSelfTutorialStageBlock& stageBlock = pastSelfTutorialStageBlocks_[blockIndex]; // 描画対象のチュートリアルステージブロック
        if (stageBlock.object) {
            stageBlock.object->Draw();
        }
    }
}

/// <summary>
/// 分身チュートリアル用の全面コライダーを追加する。
/// </summary>
void PlayScene::AppendPastSelfTutorialSolidColliders(std::vector<SolidCollider>* colliders) const
{
    if (!colliders) {
        return;
    }

    for (size_t blockIndex = 0; blockIndex < pastSelfTutorialStageBlocks_.size(); ++blockIndex) {
        if (!IsPastSelfTutorialStageBlockEnabled(blockIndex)) {
            continue;
        }
        const PastSelfTutorialStageBlock& stageBlock = pastSelfTutorialStageBlocks_[blockIndex]; // 衝突対象のチュートリアルステージブロック
        if (stageBlock.collider.enabled) {
            colliders->push_back(stageBlock.collider);
        }
    }

    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        const SolidCollider toggleGateCollider = pastSelfTutorialToggleGate_.GetSolidCollider(); // 1体用ルートのトグル連動ゲート
        if (toggleGateCollider.enabled) {
            colliders->push_back(toggleGateCollider);
        }
        const SolidCollider toggleElevatorCollider = pastSelfTutorialToggleElevator_.GetSolidCollider(); // 1体用ルートの昇降足場
        if (toggleElevatorCollider.enabled) {
            colliders->push_back(toggleElevatorCollider);
        }
        return;
    }

    const SolidCollider doorCollider = pastSelfTutorialDoor_.GetSolidCollider(); // 閉じている扉の全面コライダー
    if (doorCollider.enabled) {
        colliders->push_back(doorCollider);
    }

    const SolidCollider timedDoorCollider = pastSelfTutorialTimedDoor_.GetSolidCollider(); // 閉じている時間差扉の全面コライダー
    if (timedDoorCollider.enabled) {
        colliders->push_back(timedDoorCollider);
    }

    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        return;
    }

    const SolidCollider toggleGateCollider = pastSelfTutorialToggleGate_.GetSolidCollider(); // 閉じているトグル連動ゲートの全面コライダー
    if (toggleGateCollider.enabled) {
        colliders->push_back(toggleGateCollider);
    }

    const SolidCollider toggleElevatorCollider = pastSelfTutorialToggleElevator_.GetSolidCollider(); // 現在位置のトグル連動昇降足場コライダー
    if (toggleElevatorCollider.enabled) {
        colliders->push_back(toggleElevatorCollider);
    }

    const SolidCollider goalBridgeCollider = pastSelfTutorialGoalBridge_.GetSolidCollider(); // 展開中のゴール前の橋コライダー
    if (goalBridgeCollider.enabled) {
        colliders->push_back(goalBridgeCollider);
    }

    const SolidCollider oneWayGateCollider = pastSelfTutorialOneWayGate_.GetSolidCollider(); // 戻り方向を塞ぐ一方通行ゲートの全面コライダー
    if (oneWayGateCollider.enabled) {
        colliders->push_back(oneWayGateCollider);
    }
}

/// <summary>
/// 分身チュートリアル用ゴール判定を更新する。
/// </summary>
void PlayScene::UpdatePastSelfTutorialGoal()
{
    if (pastSelfTutorialRoute_ != PastSelfTutorialRoute::FinalChallenge ||
        pastSelfTutorialState_.gimmicks.goalReached || pastSelfRecorder_.IsRecording()) {
        return;
    }

    if (!pastSelfTutorialGoal_.Update(player_.GetState())) {
        return;
    }

    pastSelfTutorialState_.gimmicks.goalReached = true;
    pastSelfTutorialState_.history.finalChallengeCleared = true;
    RegisterPastSelfTutorialCheckCompleted("Goal reached");
    pastSelfTutorialState_.replay.clearTime = pastSelfTutorialState_.progress.elapsedTime;
    pastSelfTutorialState_.progress.lastRecordDuration = pastSelfRecorder_.GetDuration();
    pastSelfRecorder_.Stop();
    pastSelfCloneManager_.PauseAll();
    player_.SetMaterialColor(kPastSelfTutorialClearPlayerColor);
    ApplyPastSelfTutorialGoalVisual();
}

/// <summary>
/// 分身チュートリアル用ゴール表示を現在状態に合わせる。
/// </summary>
void PlayScene::ApplyPastSelfTutorialGoalVisual()
{
    for (PastSelfTutorialStageBlock& stageBlock : pastSelfTutorialStageBlocks_) {
        if (!stageBlock.object || !stageBlock.goalMarker) {
            continue;
        }

        const Math::Vector4 goalColor = pastSelfTutorialState_.gimmicks.goalReached ? kPastSelfTutorialGoalClearColor : stageBlock.baseColor; // 現在状態に応じたゴール色
        stageBlock.object->SetMaterialColor(goalColor);
    }
}

/// <summary>
/// 分身チュートリアル用オブジェクトを描画する。
/// </summary>
void PlayScene::DrawPastSelfTutorial()
{
    DrawPastSelfTutorialStage();
    DrawPastSelfTutorialMechanics();
    DrawPastSelfTutorialCloneRecordMarkers();
    pastSelfCloneManager_.Draw();
    player_.Draw();
}

/// <summary>
/// ImGuiで分身チュートリアル用の状態を表示する。
/// </summary>
void PlayScene::DrawPastSelfTutorialImGui()
{
#ifdef USE_IMGUI
    const size_t storedCloneCount = pastSelfCloneManager_.GetCloneCount(); // 現在保存している分身数
    const bool cloneSlotsFull = storedCloneCount >= pastSelfTutorialStageRules_.maxStoredClones; // 保存枠が上限へ到達したか
    const bool selectedRouteComplete = IsPastSelfTutorialSelectedRouteComplete(); // 選択中ルートがクリア済みか
    const bool allRoutesComplete = pastSelfTutorialState_.history.oneCloneRouteCleared && pastSelfTutorialState_.history.twoCloneRouteCleared &&
        pastSelfTutorialState_.history.finalChallengeCleared; // 3つのチュートリアルルートをすべてクリア済みか
    ImGui::Text("Move: A/D or Left Stick X");
    ImGui::Text("Jump: Space or GamePad A");
    ImGui::TextWrapped("C: Record next + replay stored  V: Replay stored only  B: Stop clones");
    ImGui::TextWrapped("T: Prepare replay  X: Undo last clone  R: Reset puzzle");
    if (pastSelfTutorialShowVerificationDetails_) {
        ImGui::Text("Switch: %s", pastSelfTutorialState_.gimmicks.switchActive ? "ON" : "OFF");
        ImGui::Text("Switch Source: Clone %s / Player %s", pastSelfTutorialState_.gimmicks.cloneOnSwitch ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.playerOnSwitch ? "ON" : "OFF");
        ImGui::Text("Door: %s", pastSelfTutorialState_.gimmicks.doorOpen ? "Open" : "Closed");
        ImGui::Text("Timed: Switch %s %.2f sec / Door %s", pastSelfTutorialState_.gimmicks.timedSwitchActive ? "ON" : "OFF", pastSelfTutorialTimedSwitch_.GetRemainingSeconds(), pastSelfTutorialState_.gimmicks.timedDoorOpen ? "Open" : "Closed");
        ImGui::Text("Toggle Lab: Switch %s  Clone %s / Gate %s / Lift %s Y %.2f Hold %.2f", pastSelfTutorialState_.gimmicks.toggleSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.toggleSwitchCloneOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.toggleGateOpen ? "Open" : "Closed", pastSelfTutorialToggleElevator_.IsWaitingAtEndpoint() ? "Holding" : (pastSelfTutorialState_.gimmicks.toggleElevatorActive ? "Moving" : "Idle"), pastSelfTutorialToggleElevator_.GetCurrentTranslate().y, pastSelfTutorialToggleElevator_.GetEndpointWaitRemainingSeconds());
        ImGui::Text("Weight: %s  Player %s / Clone %s  Bridge %s", pastSelfTutorialState_.gimmicks.weightSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.weightPlayerOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.weightCloneOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.goalBridgeDeployed ? "Deployed" : "Retracted");
        ImGui::Text("OneWay: %s", pastSelfTutorialState_.gimmicks.oneWayGateBlocking ? "Blocking" : "Passable");
        ImGui::Text("Route: %s  Takes: %u  Stored: %zu / %zu", GetPastSelfTutorialRouteLabel(),
            pastSelfTutorialState_.progress.recordTakeCount, storedCloneCount, pastSelfTutorialStageRules_.maxStoredClones);
    }
    ImGui::Text("Time: %.2f sec  Clear: %.2f sec  Record: %.2f sec", pastSelfTutorialState_.progress.elapsedTime, pastSelfTutorialState_.replay.clearTime, pastSelfTutorialState_.progress.lastRecordDuration);
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
    if (selectedRouteComplete) {
        ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.25f, 1.0f), "CLEAR");
    }
    ImGui::Text("Route Result: %s", selectedRouteComplete ? "Complete" : "Incomplete");
    if (cloneSlotsFull && !pastSelfRecorder_.IsRecording() && !selectedRouteComplete) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Clone slots full: press X to remove the last clone");
    }
    const bool canPrepareReplay = !selectedRouteComplete && !pastSelfRecorder_.IsRecording() &&
        storedCloneCount > 0; // 分身群を残して再生準備へ戻せるか
    ImGui::Text("Prepare: %s", selectedRouteComplete ? "Locked - reset puzzle to restart" :
        (canPrepareReplay ? "Ready - keeps stored clones" : "Locked - store a clone and stop recording"));
    if (pastSelfTutorialState_.replay.prepareFeedbackSeconds > 0.0f) {
        ImGui::TextColored(ImVec4(0.15f, 1.0f, 0.45f, 1.0f), "PREPARED: clones kept / replay ready");
    }
    if (selectedRouteComplete) {
        ImGui::BeginDisabled();
    }
    const bool disableRecordingButton = !pastSelfRecorder_.IsRecording() && !CanStartPastSelfTutorialRecording(); // 新規記録を開始できず停止操作でもないか
    if (disableRecordingButton) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(pastSelfRecorder_.IsRecording() ? "Stop Recording" : "Record Next + Replay Stored")) {
        if (pastSelfRecorder_.IsRecording()) {
            FinishPastSelfTutorialRecording();
        } else {
            StartPastSelfTutorialRecording();
        }
    }
    if (disableRecordingButton) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Replay Stored Only")) {
        if (pastSelfCloneManager_.StartAll()) {
            pastSelfTutorialState_.progress.replayStarted = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Clones")) {
        pastSelfCloneManager_.StopAll();
    }
    if (selectedRouteComplete) {
        ImGui::EndDisabled();
    }
    if (!canPrepareReplay) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Prepare Replay")) {
        ResetPastSelfTutorialReplayState();
    }
    if (!canPrepareReplay) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    const bool canUndoLastClone = !selectedRouteComplete && !pastSelfRecorder_.IsRecording() &&
        storedCloneCount > 0; // 最後の分身を削除できるか
    if (!canUndoLastClone) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Undo Last Clone")) {
        UndoLastPastSelfTutorialClone();
    }
    if (!canUndoLastClone) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Puzzle")) {
        ResetPastSelfTutorialState();
    }
    if (selectedRouteComplete) {
        ImGui::Separator();
        if (ImGui::Button("Retry Route")) {
            ResetPastSelfTutorialState();
        }
        if (pastSelfTutorialRoute_ != PastSelfTutorialRoute::FinalChallenge) {
            ImGui::SameLine();
            if (ImGui::Button("Next Route")) {
                AdvancePastSelfTutorialRoute();
            }
        }
        if (allRoutesComplete) {
            ImGui::TextColored(ImVec4(0.15f, 1.0f, 0.45f, 1.0f), "All Routes Complete");
        }
    }
    if (ImGui::CollapsingHeader("Clone Identities", ImGuiTreeNodeFlags_DefaultOpen)) {
        pastSelfCloneManager_.DrawIdentityLegendImGui();
    }
    if (pastSelfTutorialShowVerificationDetails_) {
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
/// 分身チュートリアルルートの選択UIを表示する。
/// </summary>
void PlayScene::DrawPastSelfTutorialRouteSelector()
{
#ifdef USE_IMGUI
    ImGui::Text("Route Mode");
    const bool oneCloneSelected = pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics; // 1体用ルートを選択中か
    if (ImGui::RadioButton("1 Clone", oneCloneSelected)) {
        SelectPastSelfTutorialRoute(PastSelfTutorialRoute::OneCloneBasics);
    }
    ImGui::SameLine();
    const bool twoCloneSelected = pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation; // 2体用ルートを選択中か
    if (ImGui::RadioButton("2 Clones", twoCloneSelected)) {
        SelectPastSelfTutorialRoute(PastSelfTutorialRoute::TwoCloneCooperation);
    }
    ImGui::SameLine();
    const bool finalChallengeSelected = pastSelfTutorialRoute_ == PastSelfTutorialRoute::FinalChallenge; // 最終課題を選択中か
    if (ImGui::RadioButton("Final", finalChallengeSelected)) {
        SelectPastSelfTutorialRoute(PastSelfTutorialRoute::FinalChallenge);
    }
#endif
}

/// <summary>
/// 選択されたチュートリアルルートに対応するルールを適用する。
/// </summary>
void PlayScene::SelectPastSelfTutorialRoute(PastSelfTutorialRoute route)
{
    if (pastSelfTutorialRoute_ == route) {
        return;
    }

    pastSelfTutorialRoute_ = route;
    switch (pastSelfTutorialRoute_) {
    case PastSelfTutorialRoute::OneCloneBasics:
        pastSelfTutorialStageRules_.maxStoredClones = 1;
        break;
    case PastSelfTutorialRoute::TwoCloneCooperation:
        pastSelfTutorialStageRules_.maxStoredClones = 2;
        break;
    case PastSelfTutorialRoute::FinalChallenge:
        pastSelfTutorialStageRules_.maxStoredClones = 3;
        break;
    }
    pastSelfRecorder_.SetMaxRecordTime(pastSelfTutorialStageRules_.maxRecordTime);
    ResetPastSelfTutorialState();
}

/// <summary>
/// クリア済みのルートから次のチュートリアルルートへ進む。
/// </summary>
void PlayScene::AdvancePastSelfTutorialRoute()
{
    if (!IsPastSelfTutorialSelectedRouteComplete()) {
        return;
    }

    switch (pastSelfTutorialRoute_) {
    case PastSelfTutorialRoute::OneCloneBasics:
        SelectPastSelfTutorialRoute(PastSelfTutorialRoute::TwoCloneCooperation);
        break;
    case PastSelfTutorialRoute::TwoCloneCooperation:
        SelectPastSelfTutorialRoute(PastSelfTutorialRoute::FinalChallenge);
        break;
    case PastSelfTutorialRoute::FinalChallenge:
        break;
    }
}

/// <summary>
/// 現在選択中のチュートリアルルート名を取得する。
/// </summary>
const char* PlayScene::GetPastSelfTutorialRouteLabel() const
{
    switch (pastSelfTutorialRoute_) {
    case PastSelfTutorialRoute::OneCloneBasics:
        return "One-clone tutorial";
    case PastSelfTutorialRoute::TwoCloneCooperation:
        return "Two-clone tutorial";
    case PastSelfTutorialRoute::FinalChallenge:
        return "Final challenge";
    }
    return "Unknown route";
}

/// <summary>
/// 現在選択中のチュートリアルルートが完了しているか判定する。
/// </summary>
bool PlayScene::IsPastSelfTutorialSelectedRouteComplete() const
{
    switch (pastSelfTutorialRoute_) {
    case PastSelfTutorialRoute::OneCloneBasics:
        return pastSelfTutorialState_.replay.oneCloneBasicsComplete;
    case PastSelfTutorialRoute::TwoCloneCooperation:
        return pastSelfTutorialState_.replay.twoCloneCooperationComplete;
    case PastSelfTutorialRoute::FinalChallenge:
        return pastSelfTutorialState_.gimmicks.goalReached;
    }
    return false;
}

/// <summary>
/// 選択中のチュートリアルルートのクリア状態を確定する。
/// </summary>
void PlayScene::FinalizePastSelfTutorialSelectedRoute()
{
    const bool isTutorialRoute = pastSelfTutorialRoute_ != PastSelfTutorialRoute::FinalChallenge; // チュートリアル専用の完了処理を行うか
    if (!isTutorialRoute || pastSelfTutorialState_.replay.routeClearFinalized || !IsPastSelfTutorialSelectedRouteComplete()) {
        return;
    }

    pastSelfTutorialState_.replay.routeClearFinalized = true;
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        pastSelfTutorialState_.history.oneCloneRouteCleared = true;
    } else if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        pastSelfTutorialState_.history.twoCloneRouteCleared = true;
    }
    pastSelfTutorialState_.replay.clearTime = pastSelfTutorialState_.progress.elapsedTime;
    pastSelfTutorialState_.progress.lastRecordDuration = pastSelfRecorder_.GetDuration();
    pastSelfRecorder_.Stop();
    pastSelfTutorialState_.replay.recordingPendingCommit = false;
    pastSelfCloneManager_.PauseAll();
    player_.SetMaterialColor(kPastSelfTutorialClearPlayerColor);
}

/// <summary>
/// 現在の攻略状態から次に行う確認手順を取得する。
/// </summary>
const char* PlayScene::GetPastSelfTutorialNextActionText() const
{
    const size_t storedCloneCount = pastSelfCloneManager_.GetCloneCount(); // 保存済みの分身数
    const size_t visibleCloneCount = pastSelfCloneManager_.GetVisibleCount(); // 表示中の分身数
    const bool hasStoredClones = storedCloneCount > 0; // 再生に使用できる分身があるか
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        if (pastSelfTutorialState_.replay.oneCloneBasicsComplete) {
            return "One-clone tutorial route complete";
        }
        if (pastSelfRecorder_.IsRecording()) {
            return "Record the clone moving onto the orange toggle";
        }
        if (!hasStoredClones) {
            return "Press C and record one clone on the orange toggle";
        }
        if (!pastSelfTutorialState_.progress.prepareUsed) {
            return "Press T to prepare the one-clone replay";
        }
        if (visibleCloneCount == 0) {
            return "Press V to replay the stored clone";
        }
        if (!pastSelfTutorialState_.replay.oneCloneToggleActivated) {
            return "Wait for the clone to activate the orange toggle";
        }
        if (!pastSelfTutorialState_.replay.oneCloneElevatorRidden) {
            return "Move left and board the orange moving lift";
        }
        return "Ride the orange moving lift to the upper endpoint";
    }
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        if (pastSelfTutorialState_.replay.twoCloneCooperationComplete) {
            return "Two-clone tutorial route complete";
        }
        if (pastSelfRecorder_.IsRecording()) {
            return storedCloneCount == 0 ? "Record the first clone role" : "Record the second clone role";
        }
        if (storedCloneCount < 2) {
            return storedCloneCount == 0 ? "Press C to record the first clone role" : "Press T, then C to record the second clone role";
        }
        if (!pastSelfTutorialState_.replay.twoCloneReplayPrepared) {
            return "Press T to prepare the two-clone replay";
        }
        if (visibleCloneCount == 0) {
            return "Press V to replay both stored clones";
        }
        if (!pastSelfTutorialState_.replay.twoCloneSwitchesActivated) {
            return "Keep separate clones on the green and blue switches";
        }
        return "Move right through the opened blue door";
    }
    const PastSelfTutorialStatus status = pastSelfTutorialState_.CalculateStatus(storedCloneCount); // 表示と手順案内で共有する達成状況

    if (pastSelfTutorialState_.gimmicks.goalReached) {
        if (status.multiCloneRouteComplete) {
            return "Multi-clone route complete";
        }
        if (!status.allChecksComplete) {
            return "Goal reached; verification checks still missing";
        }
        if (!status.videoFlowComplete) {
            return "Goal reached; video proof flow still missing";
        }
        if (!status.enoughStoredClones) {
            return "Goal reached; store 2 or more clones";
        }
        return "Goal reached; multi-clone proof still missing";
    }
    if (pastSelfRecorder_.IsRecording()) {
        return "Recording next clone; stored clones replay automatically";
    }
    if (pastSelfTutorialStageRules_.maxStoredClones == 0) {
        return "No clone slots configured for this stage";
    }
    if (!hasStoredClones) {
        return "Press C to record the first clone role";
    }
    if (!pastSelfTutorialState_.progress.prepareUsed) {
        return "Press T to prepare replay with records kept";
    }
    if (!pastSelfTutorialState_.progress.doorBlockedBeforeClone) {
        return "Show the closed green door blocks the player";
    }
    if (visibleCloneCount == 0) {
        return "Press V to replay stored clones only (no new recording)";
    }
    if (pastSelfTutorialState_.gimmicks.playerOnSwitch && !pastSelfTutorialState_.gimmicks.cloneOnSwitch) {
        return "Move the clone onto the green switch";
    }
    if (!pastSelfTutorialState_.progress.doorOpenedByClone) {
        return "Wait for the clone to open the green door";
    }
    if (!pastSelfTutorialState_.progress.clonePlatformUsed) {
        return "Use a clone as the blue-step platform";
    }
    if (!pastSelfTutorialState_.progress.dualCloneSwitchesActivated) {
        return "Keep clones on the green and blue switches";
    }
    if (!pastSelfTutorialState_.progress.timedDoorOpened) {
        return "Pass through the opened blue door";
    }
    if (!pastSelfTutorialState_.progress.weightSwitchActivated) {
        return "Activate the yellow switch with player and clone";
    }
    if (!pastSelfTutorialState_.progress.oneWayGateUsed) {
        return "Pass the purple gate and test the return path";
    }
    return "Reach the goal";
}

/// <summary>
/// 新たに達成した検証項目をHUD通知へ登録する。
/// </summary>
void PlayScene::RegisterPastSelfTutorialCheckCompleted(const char* checkText)
{
    if (!checkText || checkText[0] == '\0') {
        return;
    }

    pastSelfTutorialState_.progress.recentCheckText = checkText;
    pastSelfTutorialState_.progress.recentCheckSeconds = kCompletedCheckFeedbackDuration;
}

/// <summary>
/// プレイヤー操作に必要な主要状態を固定表示する。
/// </summary>
void PlayScene::DrawPastSelfTutorialFixedStatusHud()
{
#ifdef USE_IMGUI
    const ImVec4 checkedColor = ImVec4(0.15f, 1.0f, 0.45f, 1.0f); // 達成済み状態の表示色
    const ImVec4 uncheckedColor = ImVec4(1.0f, 0.35f, 0.25f, 1.0f); // 未達成状態の表示色
    const size_t storedCloneCount = pastSelfCloneManager_.GetCloneCount(); // 保存済みの分身数
    const size_t visibleCloneCount = pastSelfCloneManager_.GetVisibleCount(); // 表示中の分身数
    const size_t playingCloneCount = pastSelfCloneManager_.GetPlayingCount(); // 再生中の分身数
    const bool selectedRouteComplete = IsPastSelfTutorialSelectedRouteComplete(); // 選択中ルートがクリア済みか
    const PastSelfTutorialStatus status = pastSelfTutorialState_.CalculateStatus(storedCloneCount); // 表示と手順案内で共有する達成状況
    const char* nextActionText = GetPastSelfTutorialNextActionText(); // 通常表示でも確認できる次の攻略手順
    const float summaryLineCount = pastSelfTutorialRoute_ == PastSelfTutorialRoute::FinalChallenge ? 8.0f : 6.0f; // 選択ルートに必要な固定表示行数
    const float summaryHeight = ImGui::GetTextLineHeightWithSpacing() * summaryLineCount +
        ImGui::GetStyle().WindowPadding.y * 2.0f; // 固定サマリー領域の高さ

    ImGui::BeginChild("PlayerFixedStatus", ImVec2(0.0f, summaryHeight), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::Text("%s", GetPastSelfTutorialRouteLabel());
    ImGui::SameLine();
    ImGui::Text("Record: %s", pastSelfRecorder_.IsRecording() ? "Recording" : "Stopped");
    ImGui::Text("Takes: %u  Stored: %zu/%zu  Visible: %zu  Playing: %zu",
        pastSelfTutorialState_.progress.recordTakeCount, storedCloneCount, pastSelfTutorialStageRules_.maxStoredClones,
        visibleCloneCount, playingCloneCount);
    ImGui::Text("Routes: 1 %s  2 %s  Final %s",
        pastSelfTutorialState_.history.oneCloneRouteCleared ? "[x]" : "[ ]",
        pastSelfTutorialState_.history.twoCloneRouteCleared ? "[x]" : "[ ]",
        pastSelfTutorialState_.history.finalChallengeCleared ? "[x]" : "[ ]");
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        ImGui::TextColored(pastSelfTutorialState_.replay.oneCloneBasicsComplete ? checkedColor : uncheckedColor,
            "One-clone tutorial: %d / 4%s", status.oneCloneTutorialCheckCount,
            pastSelfTutorialState_.replay.oneCloneBasicsComplete ? " Complete" : "");
        if (selectedRouteComplete) {
            ImGui::TextColored(checkedColor, "CLEAR %.2f sec", pastSelfTutorialState_.replay.clearTime);
        } else {
            ImGui::TextWrapped("Next: %s", nextActionText);
        }
        ImGui::EndChild();
        return;
    }
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        ImGui::TextColored(pastSelfTutorialState_.replay.twoCloneCooperationComplete ? checkedColor : uncheckedColor,
            "Two-clone tutorial: %d / 4%s", status.twoCloneTutorialCheckCount,
            pastSelfTutorialState_.replay.twoCloneCooperationComplete ? " Complete" : "");
        if (selectedRouteComplete) {
            ImGui::TextColored(checkedColor, "CLEAR %.2f sec", pastSelfTutorialState_.replay.clearTime);
        } else {
            ImGui::TextWrapped("Next: %s", nextActionText);
        }
        ImGui::EndChild();
        return;
    }
    if (pastSelfTutorialState_.gimmicks.goalReached) {
        ImGui::TextColored(checkedColor, "Goal Reached / CLEAR");
        ImGui::TextColored(status.allChecksComplete ? checkedColor : uncheckedColor, "%d / 7%s",
            status.completedCheckCount, status.allChecksComplete ? " All complete" : " incomplete");
        ImGui::SameLine();
        ImGui::TextColored(status.videoFlowComplete ? checkedColor : uncheckedColor, "Video: %d / 5%s",
            status.completedFlowCount, status.videoFlowComplete ? " Flow complete" : "");
        ImGui::TextColored(status.multiCloneRouteComplete ? checkedColor : uncheckedColor, "%s",
            status.multiCloneRouteComplete ? "Multi-clone route complete" : nextActionText);
        if (status.allRoutesComplete) {
            ImGui::TextColored(checkedColor, "All Routes Complete");
        }
    } else {
        ImGui::TextColored(uncheckedColor, "Goal: Not Reached");
        ImGui::SameLine();
        ImGui::TextColored(uncheckedColor, "Route: Incomplete");
        if (pastSelfTutorialShowVerificationDetails_) {
            ImGui::TextColored(status.allChecksComplete ? checkedColor : uncheckedColor, "Checks: %d / 7%s",
                status.completedCheckCount, status.allChecksComplete ? " All complete" : "");
            ImGui::SameLine();
            ImGui::TextColored(status.videoFlowComplete ? checkedColor : uncheckedColor, "Video: %d / 5%s",
                status.completedFlowCount, status.videoFlowComplete ? " Flow complete" : "");
        } else {
            ImGui::Text("Progress: %d / 7", status.completedCheckCount);
            ImGui::SameLine();
            ImGui::TextDisabled("Mode: Play");
        }
        if (pastSelfTutorialState_.progress.recentCheckSeconds > 0.0f && !pastSelfTutorialState_.progress.recentCheckText.empty()) {
            ImGui::TextColored(checkedColor, "Completed: %s", pastSelfTutorialState_.progress.recentCheckText.c_str());
        } else {
            ImGui::TextWrapped("Next: %s", nextActionText);
        }
    }
    ImGui::EndChild();
#endif
}

/// <summary>
/// 分身チュートリアル用の検証状態をPlayerタブ内に表示する。
/// </summary>
void PlayScene::DrawPastSelfTutorialStatusHud()
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
    const bool selectedRouteComplete = IsPastSelfTutorialSelectedRouteComplete(); // 選択中ルートがクリア済みか
    const bool canPrepareReplay = !selectedRouteComplete && !pastSelfRecorder_.IsRecording() && hasStoredClones; // Prepare操作を受け付けられるか
    const char* routeLabel = GetPastSelfTutorialRouteLabel(); // 現在のチュートリアルルート種別
    const char* routeNeedText = "Stored >= 2 and Green + Blue active together"; // 選択ルートの主要完了条件
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        routeNeedText = "Takes 1 / Stored 1 / Orange toggle + moving lift";
    } else if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        routeNeedText = "Takes 2 / Stored 2 / Green + Blue / pass blue door";
    }
    const char* phaseLabel = "Reset / no record"; // 現在の検証フェーズ表示
    const PastSelfTutorialStatus status = pastSelfTutorialState_.CalculateStatus(storedCloneCount); // 表示と手順案内で共有する達成状況
    if (selectedRouteComplete) {
        phaseLabel = "Route clear";
    } else if (pastSelfRecorder_.IsRecording()) {
        phaseLabel = "Recording";
    } else if (hasStoredClones && visibleCloneCount == 0) {
        phaseLabel = "Replay ready after Prepare";
    } else if (playingCloneCount > 0) {
        phaseLabel = "Clones playing";
    } else if (visibleCloneCount > 0) {
        phaseLabel = "Clones finished";
    }

    const char* nextActionText = GetPastSelfTutorialNextActionText(); // HUDに表示する次の確認手順

    ImGui::Text("Past Self Tutorial");
    ImGui::Separator();
    ImGui::TextWrapped("C Record next + replay stored | V Replay stored only | B Stop clones");
    ImGui::TextWrapped("T Prepare replay | X Undo last clone | R Reset puzzle");
    ImGui::Text("Phase : %s", phaseLabel);
    ImGui::Text("Time  : %.2f sec  Clear %.2f sec", pastSelfTutorialState_.progress.elapsedTime, pastSelfTutorialState_.replay.clearTime);
    ImGui::Text("Route : %s  Takes: %u", routeLabel, pastSelfTutorialState_.progress.recordTakeCount);
    ImGui::TextColored(selectedRouteComplete ? checkedColor : uncheckedColor, "Result: %s",
        selectedRouteComplete ? "CLEAR" : "Incomplete");
    ImGui::Text("Routes: 1 %s  2 %s  Final %s",
        pastSelfTutorialState_.history.oneCloneRouteCleared ? "[x]" : "[ ]",
        pastSelfTutorialState_.history.twoCloneRouteCleared ? "[x]" : "[ ]",
        pastSelfTutorialState_.history.finalChallengeCleared ? "[x]" : "[ ]");
    if (status.allRoutesComplete) {
        ImGui::TextColored(checkedColor, "All Routes Complete");
    }
    ImGui::Text("Need  : %s", routeNeedText);
    ImGui::Text("Record: %s  Frames: %zu  %.2f sec", recordStateLabel, pastSelfRecorder_.GetFrames().size(), pastSelfTutorialState_.progress.lastRecordDuration);
    ImGui::Text("Clones: Stored %zu/%zu  Visible %zu  Playing %zu", storedCloneCount,
        pastSelfTutorialStageRules_.maxStoredClones, visibleCloneCount, playingCloneCount);
    ImGui::Text("Prepare: %s", selectedRouteComplete ? "Locked after clear" : (canPrepareReplay ? "Ready" : "Locked"));
    if (pastSelfTutorialState_.replay.prepareFeedbackSeconds > 0.0f) {
        ImGui::TextColored(checkedColor, "PREPARED: start position / records kept");
    }
    ImGui::Text("Switch: %s  Clone %s  Player %s",
        pastSelfTutorialState_.gimmicks.switchActive ? "ON" : "OFF",
        pastSelfTutorialState_.gimmicks.cloneOnSwitch ? "ON" : "OFF",
        pastSelfTutorialState_.gimmicks.playerOnSwitch ? "ON" : "OFF");
    ImGui::Text("Door  : %s  Timed %s  Goal %s%s", pastSelfTutorialState_.gimmicks.doorOpen ? "Open" : "Closed", pastSelfTutorialState_.gimmicks.timedDoorOpen ? "Open" : "Closed", pastSelfTutorialState_.gimmicks.goalReached ? "Reached" : "Not Reached", pastSelfTutorialState_.gimmicks.goalReached ? " / CLEAR" : "");
    ImGui::Text("Timed: %s  Clone %s  %.2f sec", pastSelfTutorialState_.gimmicks.timedSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.timedSwitchCloneOn ? "ON" : "OFF", pastSelfTutorialTimedSwitch_.GetRemainingSeconds());
    ImGui::Text("Toggle: %s  Clone %s  Gate %s  Lift %s Y %.2f Hold %.2f", pastSelfTutorialState_.gimmicks.toggleSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.toggleSwitchCloneOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.toggleGateOpen ? "Open" : "Closed", pastSelfTutorialToggleElevator_.IsWaitingAtEndpoint() ? "Holding" : (pastSelfTutorialState_.gimmicks.toggleElevatorActive ? "Moving" : "Idle"), pastSelfTutorialToggleElevator_.GetCurrentTranslate().y, pastSelfTutorialToggleElevator_.GetEndpointWaitRemainingSeconds());
    ImGui::Text("Weight: %s  Player %s  Clone %s  Bridge %s", pastSelfTutorialState_.gimmicks.weightSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.weightPlayerOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.weightCloneOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.goalBridgeDeployed ? "Deployed" : "Retracted");
    ImGui::Text("OneWay: %s", pastSelfTutorialState_.gimmicks.oneWayGateBlocking ? "Blocking" : "Passable");
    ImGui::Text("Next  : %s", nextActionText);
    ImGui::Text("Checks: %d / 7%s", status.completedCheckCount, status.allChecksComplete ? " All complete" : "");
    ImGui::Text("Video : %d / 5%s", status.completedFlowCount, status.videoFlowComplete ? " Flow complete" : "");
    ImGui::TextColored(normalDoorColor, "Green : Clone switch door");
    ImGui::TextColored(timedDoorColor, "Blue  : Green + timed switch door");
    ImGui::TextColored(toggleSwitchColor, "Orange: Optional toggle gate + moving lift");
    ImGui::TextColored(weightSwitchColor, "Yellow: Weight switch + goal bridge");
    ImGui::TextColored(oneWayGateColor, "Purple: One-way return block");
    ImGui::Separator();
    ImGui::Text("Video proof flow");
    ImGui::TextColored(pastSelfTutorialState_.progress.resetShown ? checkedColor : uncheckedColor, "[%c] Reset state shown", pastSelfTutorialState_.progress.resetShown ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.recordStarted ? checkedColor : uncheckedColor, "[%c] Recording started", pastSelfTutorialState_.progress.recordStarted ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.recordStopped ? checkedColor : uncheckedColor, "[%c] Recording stopped", pastSelfTutorialState_.progress.recordStopped ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.prepareUsed ? checkedColor : uncheckedColor, "[%c] Prepare returned with record kept", pastSelfTutorialState_.progress.prepareUsed ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.replayStarted ? checkedColor : uncheckedColor, "[%c] Recorded clone replay started", pastSelfTutorialState_.progress.replayStarted ? 'x' : ' ');
    ImGui::Separator();
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
        ImGui::Text("One-clone tutorial");
        ImGui::TextColored(status.oneCloneRecordingStored ? checkedColor : uncheckedColor, "[%c] One recording stored", status.oneCloneRecordingStored ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.oneCloneToggleActivated ? checkedColor : uncheckedColor, "[%c] Clone activated orange toggle", pastSelfTutorialState_.replay.oneCloneToggleActivated ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.oneCloneElevatorRidden ? checkedColor : uncheckedColor, "[%c] Player rode orange moving lift", pastSelfTutorialState_.replay.oneCloneElevatorRidden ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.oneCloneBasicsComplete ? checkedColor : uncheckedColor, "[%c] Upper endpoint reached", pastSelfTutorialState_.replay.oneCloneBasicsComplete ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.oneCloneBasicsComplete ? checkedColor : uncheckedColor, "%s",
            pastSelfTutorialState_.replay.oneCloneBasicsComplete ? "One-clone tutorial route complete" : "One-clone tutorial route incomplete");
        ImGui::Separator();
    }
    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
        ImGui::Text("Two-clone tutorial");
        ImGui::TextColored(status.twoCloneRecordingsStored ? checkedColor : uncheckedColor, "[%c] Two recordings stored", status.twoCloneRecordingsStored ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.twoCloneReplayPrepared ? checkedColor : uncheckedColor, "[%c] Prepare kept two records", pastSelfTutorialState_.replay.twoCloneReplayPrepared ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.twoCloneSwitchesActivated ? checkedColor : uncheckedColor, "[%c] Separate clones activated Green + Blue", pastSelfTutorialState_.replay.twoCloneSwitchesActivated ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.twoCloneCooperationComplete ? checkedColor : uncheckedColor, "[%c] Player passed blue door", pastSelfTutorialState_.replay.twoCloneCooperationComplete ? 'x' : ' ');
        ImGui::TextColored(pastSelfTutorialState_.replay.twoCloneCooperationComplete ? checkedColor : uncheckedColor, "%s",
            pastSelfTutorialState_.replay.twoCloneCooperationComplete ? "Two-clone tutorial route complete" : "Two-clone tutorial route incomplete");
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Implementation Proof", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Runtime-owned classes");
        ImGui::BulletText("Clone manager: %s", typeid(pastSelfCloneManager_).name());
        ImGui::BulletText("Stored units : %s x %zu", typeid(PastSelfClone).name(), storedCloneCount);
        ImGui::BulletText("Green route : %s -> %s",
            typeid(pastSelfTutorialSwitch_).name(), typeid(pastSelfTutorialDoor_).name());
        ImGui::BulletText("Blue route  : %s + %s",
            typeid(pastSelfTutorialSwitch_).name(), typeid(pastSelfTutorialTimedSwitch_).name());
        ImGui::BulletText("Toggle lab  : %s -> %s + %s",
            typeid(pastSelfTutorialToggleSwitch_).name(), typeid(pastSelfTutorialToggleGate_).name(), typeid(pastSelfTutorialToggleElevator_).name());
        ImGui::BulletText("Weight route: %s -> %s", typeid(pastSelfTutorialWeightSwitch_).name(), typeid(pastSelfTutorialGoalBridge_).name());
        ImGui::BulletText("Return route: %s", typeid(pastSelfTutorialOneWayGate_).name());
        ImGui::BulletText("Goal        : %s", typeid(pastSelfTutorialGoal_).name());
        ImGui::Text("Live connections");
        ImGui::BulletText("Green switch %s -> Linked door %s",
            pastSelfTutorialState_.gimmicks.switchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.doorOpen ? "Open" : "Closed");
        ImGui::BulletText("Green %s + Timed %s -> Blue door %s",
            pastSelfTutorialState_.gimmicks.switchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.timedSwitchActive ? "ON" : "OFF",
            pastSelfTutorialState_.gimmicks.timedDoorOpen ? "Open" : "Closed");
        ImGui::BulletText("Toggle %s / Clone %s -> Gate %s / Lift %s Y %.2f",
            pastSelfTutorialState_.gimmicks.toggleSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.toggleSwitchCloneOn ? "ON" : "OFF",
            pastSelfTutorialState_.gimmicks.toggleGateOpen ? "Open" : "Closed", pastSelfTutorialState_.gimmicks.toggleElevatorActive ? "Moving" : "Idle",
            pastSelfTutorialToggleElevator_.GetCurrentTranslate().y);
        ImGui::BulletText("Player %s + Clone %s -> Weight %s -> Bridge %s",
            pastSelfTutorialState_.gimmicks.weightPlayerOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.weightCloneOn ? "ON" : "OFF",
            pastSelfTutorialState_.gimmicks.weightSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.goalBridgeDeployed ? "Deployed" : "Retracted");
        ImGui::BulletText("One-way %s / Goal %s",
            pastSelfTutorialState_.gimmicks.oneWayGateBlocking ? "Blocking" : "Passable",
            pastSelfTutorialState_.gimmicks.goalReached ? "Reached" : "Not Reached");
        ImGui::TextDisabled("Source: application/player/PastSelfCloneManager.*");
        ImGui::TextDisabled("Source: application/gimmicks/StageGimmicks.*");
    }
    ImGui::Separator();
    ImGui::Text("Gimmick Debug");
    ImGui::Text("Units : BoxSwitch / LinkedDoor / TimedSwitch / ToggleSwitch / MovingPlatform / WeightSwitch / LinkedBridge / OneWayGate / Goal");
    ImGui::TextColored(normalDoorColor, "Green : BoxSwitch Clone %s -> LinkedDoor %s", pastSelfTutorialState_.gimmicks.cloneOnSwitch ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.doorOpen ? "Open" : "Closed");
    ImGui::TextColored(timedDoorColor, "Blue  : Green %s + TimedSwitch %s %.2f sec -> TimedDoor %s", pastSelfTutorialState_.gimmicks.switchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.timedSwitchActive ? "ON" : "OFF", pastSelfTutorialTimedSwitch_.GetRemainingSeconds(), pastSelfTutorialState_.gimmicks.timedDoorOpen ? "Open" : "Closed");
    ImGui::TextColored(toggleSwitchColor, "Orange: ToggleSwitch %s Clone %s -> Gate %s / Lift %s Y %.2f Hold %.2f", pastSelfTutorialState_.gimmicks.toggleSwitchActive ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.toggleSwitchCloneOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.toggleGateOpen ? "Open" : "Closed", pastSelfTutorialToggleElevator_.IsWaitingAtEndpoint() ? "Holding" : (pastSelfTutorialState_.gimmicks.toggleElevatorActive ? "Moving" : "Idle"), pastSelfTutorialToggleElevator_.GetCurrentTranslate().y, pastSelfTutorialToggleElevator_.GetEndpointWaitRemainingSeconds());
    ImGui::TextColored(weightSwitchColor, "Yellow: WeightSwitch Player %s + Clone %s -> Bridge %s", pastSelfTutorialState_.gimmicks.weightPlayerOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.weightCloneOn ? "ON" : "OFF", pastSelfTutorialState_.gimmicks.goalBridgeDeployed ? "Deployed" : "Retracted");
    ImGui::TextColored(oneWayGateColor, "Purple: OneWayGate %s", pastSelfTutorialState_.gimmicks.oneWayGateBlocking ? "Return blocked" : "Passable");
    ImGui::Separator();
    ImGui::TextColored(pastSelfTutorialState_.progress.doorBlockedBeforeClone ? checkedColor : uncheckedColor, "[%c] Closed door blocked player", pastSelfTutorialState_.progress.doorBlockedBeforeClone ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.doorOpenedByClone ? checkedColor : uncheckedColor, "[%c] Clone opened door switch", pastSelfTutorialState_.progress.doorOpenedByClone ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.clonePlatformUsed ? checkedColor : uncheckedColor, "[%c] Player used clone as platform", pastSelfTutorialState_.progress.clonePlatformUsed ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.timedDoorOpened ? checkedColor : uncheckedColor, "[%c] Green and timed switches opened blue door", pastSelfTutorialState_.progress.timedDoorOpened ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.weightSwitchActivated ? checkedColor : uncheckedColor, "[%c] Player and clone activated weight switch", pastSelfTutorialState_.progress.weightSwitchActivated ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.oneWayGateUsed ? checkedColor : uncheckedColor, "[%c] One-way gate blocked return path", pastSelfTutorialState_.progress.oneWayGateUsed ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.gimmicks.goalReached ? checkedColor : uncheckedColor, "[%c] Goal reached", pastSelfTutorialState_.gimmicks.goalReached ? 'x' : ' ');
    ImGui::Separator();
    ImGui::Text("Multi-clone proof");
    ImGui::TextColored(status.enoughStoredClones ? checkedColor : uncheckedColor, "[%c] Two or more clones stored", status.enoughStoredClones ? 'x' : ' ');
    ImGui::TextColored(pastSelfTutorialState_.progress.dualCloneSwitchesActivated ? checkedColor : uncheckedColor, "[%c] Separate clones activated Green + Blue", pastSelfTutorialState_.progress.dualCloneSwitchesActivated ? 'x' : ' ');
    if (pastSelfTutorialState_.gimmicks.goalReached) {
        ImGui::Separator();
        ImGui::TextColored(checkedColor, "Goal Reached / CLEAR");
        ImGui::TextColored(status.allChecksComplete ? checkedColor : uncheckedColor, "%d / 7%s", status.completedCheckCount,
            status.allChecksComplete ? " All complete" : " incomplete");
        ImGui::TextColored(status.videoFlowComplete ? checkedColor : uncheckedColor, "%s", status.videoFlowComplete ? "Video flow complete" : "Video flow incomplete");
        ImGui::TextColored(status.multiCloneRouteComplete ? checkedColor : uncheckedColor, "%s", status.multiCloneRouteComplete ? "Multi-clone route complete" : "Multi-clone route incomplete");
        ImGui::TextColored(status.allChecksComplete ? checkedColor : uncheckedColor, "%s", status.allChecksComplete ? "All verification checks complete" : "Verification checks still missing");
        ImGui::TextColored(checkedColor, "Clear %.2f sec / Record %.2f sec", pastSelfTutorialState_.replay.clearTime, pastSelfTutorialState_.progress.lastRecordDuration);
    }
#endif
}
