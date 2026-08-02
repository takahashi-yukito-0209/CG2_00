#include "PlayScene.h"
#include "ImGuiManager.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/io/InputManager.h"

#include <cmath>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr const char* kSimpleSkinModelFileName = "simpleSkin/simpleSkin.gltf"; // Skinning確認用simpleSkinモデル
constexpr const char* kHumanSneakWalkModelFileName = "human/sneakWalk.gltf"; // Skinning確認用sneakWalkモデル
constexpr const char* kHumanWalkModelFileName = "human/walk.gltf"; // Skinning確認用walkモデル
constexpr uint8_t kEvaluationAnimationToggleKey = DIK_P; // アニメーション再生切り替えキー
constexpr uint8_t kEvaluationAnimationSpeedDownKey = DIK_U; // アニメーション速度低下キー
constexpr uint8_t kEvaluationAnimationSpeedUpKey = DIK_I; // アニメーション速度上昇キー
constexpr uint8_t kEvaluationAnimationResetKey = DIK_O; // アニメーションリセットキー
constexpr WORD kEvaluationAnimationToggleButton = XINPUT_GAMEPAD_A; // アニメーション再生切り替えボタン
constexpr WORD kEvaluationAnimationSpeedDownButton = XINPUT_GAMEPAD_LEFT_SHOULDER; // アニメーション速度低下ボタン
constexpr WORD kEvaluationAnimationSpeedUpButton = XINPUT_GAMEPAD_RIGHT_SHOULDER; // アニメーション速度上昇ボタン
constexpr WORD kEvaluationAnimationResetButton = XINPUT_GAMEPAD_X; // アニメーションリセットボタン
constexpr float kEvaluationAnimationPlaybackSpeedStep = 0.25f; // アニメーション速度の変更幅
constexpr float kSkinningModelMoveSpeed = 3.0f; // Skinningモデルの移動速度

/// <summary>
/// ImGui操作中にゲーム側ショートカットを止める必要があるか判定する。
/// </summary>
bool ShouldBlockGameShortcutInput()
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
/// 評価確認用にパッドで動かす対象モデルか判定する。
/// </summary>
bool IsEvaluationSkinningControlTargetModelFile(const std::string& modelFileName)
{
    return modelFileName == kHumanWalkModelFileName
        || modelFileName == kHumanSneakWalkModelFileName
        || modelFileName == kSimpleSkinModelFileName;
}

/// <summary>
/// 移動入力を正規化する。
/// </summary>
Vector3 NormalizeMoveInput(const Vector3& moveInput)
{
    const float length = std::sqrt(moveInput.x * moveInput.x + moveInput.y * moveInput.y + moveInput.z * moveInput.z); // 入力ベクトルの長さ
    if (length <= 0.0001f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    if (length <= 1.0f) {
        return moveInput;
    }

    return { moveInput.x / length, moveInput.y / length, moveInput.z / length };
}
} // namespace

/// <summary>
/// 評価確認用のアニメーション操作入力を処理する。
/// </summary>
void PlayScene::HandleEvaluationAnimationInput()
{
    if (ShouldBlockGameShortcutInput()) {
        return;
    }

    InputManager* inputManager = InputManager::GetInstance(); // 評価確認用入力を取得する入力管理
    if (!inputManager) {
        return;
    }

    if (inputManager->IsKeyJustPressed(kEvaluationAnimationToggleKey) || inputManager->IsGamePadButtonJustPressed(kEvaluationAnimationToggleButton)) {
        ToggleSceneAnimationEnabled();
    }
    if (inputManager->IsKeyJustPressed(kEvaluationAnimationSpeedDownKey) || inputManager->IsGamePadButtonJustPressed(kEvaluationAnimationSpeedDownButton)) {
        AdjustSceneAnimationPlaybackSpeed(-kEvaluationAnimationPlaybackSpeedStep);
    }
    if (inputManager->IsKeyJustPressed(kEvaluationAnimationSpeedUpKey) || inputManager->IsGamePadButtonJustPressed(kEvaluationAnimationSpeedUpButton)) {
        AdjustSceneAnimationPlaybackSpeed(kEvaluationAnimationPlaybackSpeedStep);
    }
    if (inputManager->IsKeyJustPressed(kEvaluationAnimationResetKey) || inputManager->IsGamePadButtonJustPressed(kEvaluationAnimationResetButton)) {
        ResetSceneAnimations();
    }
}

/// <summary>
/// 評価確認用のSkinningモデル移動入力を処理する。
/// </summary>
void PlayScene::HandleSkinningModelControlInput(float deltaTime)
{
    Object3d* controlTarget = FindEvaluationSkinningControlObject(); // 操作対象のSkinningモデル
    InputManager* inputManager = InputManager::GetInstance(); // 移動入力を取得する入力管理
    if (!controlTarget || !inputManager) {
        return;
    }

    Vector3 moveInput { 0.0f, 0.0f, 0.0f }; // キーボードとパッドを合成した移動入力
    if (inputManager->IsKeyPressed(DIK_A)) {
        moveInput.x -= 1.0f;
    }
    if (inputManager->IsKeyPressed(DIK_D)) {
        moveInput.x += 1.0f;
    }
    if (inputManager->IsKeyPressed(DIK_W)) {
        moveInput.z += 1.0f;
    }
    if (inputManager->IsKeyPressed(DIK_S)) {
        moveInput.z -= 1.0f;
    }

    moveInput.x += inputManager->GetGamePadLeftStickX();
    moveInput.z += inputManager->GetGamePadLeftStickY();
    const Vector3 normalizedMoveInput = NormalizeMoveInput(moveInput); // 斜め移動を補正した移動入力
    if (normalizedMoveInput.x == 0.0f && normalizedMoveInput.z == 0.0f) {
        return;
    }

    Vector3 translate = controlTarget->GetTranslate(); // 操作対象の現在位置
    translate.x += normalizedMoveInput.x * kSkinningModelMoveSpeed * deltaTime;
    translate.z += normalizedMoveInput.z * kSkinningModelMoveSpeed * deltaTime;
    controlTarget->SetTranslate(translate);
}

/// <summary>
/// 評価確認用に操作するSkinningモデルを取得する。
/// </summary>
Object3d* PlayScene::FindEvaluationSkinningControlObject() const
{
    for (const auto& object3d : objects3d_) {
        if (object3d && object3d->GetDebugName() == kHumanWalkModelFileName) {
            return object3d.get();
        }
    }
    for (const auto& object3d : objects3d_) {
        if (object3d && IsEvaluationSkinningControlTargetModelFile(object3d->GetDebugName())) {
            return object3d.get();
        }
    }

    return nullptr;
}

/// <summary>
/// シーン内アニメーションの再生有効状態をまとめて設定する。
/// </summary>
void PlayScene::SetSceneAnimationEnabled(bool enabled)
{
    for (auto& object3d : objects3d_) {
        if (object3d && object3d->HasAnimation()) {
            object3d->SetAnimationEnabled(enabled);
        }
    }
}

/// <summary>
/// シーン内アニメーションの再生有効状態を切り替える。
/// </summary>
void PlayScene::ToggleSceneAnimationEnabled()
{
    bool enableAnimation = true; // 次に設定する再生状態
    for (const auto& object3d : objects3d_) {
        if (object3d && object3d->HasAnimation()) {
            enableAnimation = !object3d->GetAnimationEnabled();
            break;
        }
    }

    SetSceneAnimationEnabled(enableAnimation);
}

/// <summary>
/// シーン内アニメーションの再生速度をまとめて変更する。
/// </summary>
void PlayScene::AdjustSceneAnimationPlaybackSpeed(float speedDelta)
{
    for (auto& object3d : objects3d_) {
        if (object3d && object3d->HasAnimation()) {
            const float playbackSpeed = object3d->GetAnimationPlaybackSpeed() + speedDelta; // 変更後の再生速度
            object3d->SetAnimationPlaybackSpeed(playbackSpeed);
        }
    }
}

/// <summary>
/// シーン内アニメーションを先頭へ戻す。
/// </summary>
void PlayScene::ResetSceneAnimations()
{
    for (auto& object3d : objects3d_) {
        if (object3d && object3d->HasAnimation()) {
            object3d->ResetAnimation();
        }
    }
}

/// <summary>
/// 評価確認用操作のImGuiを描画する。
/// </summary>
void PlayScene::DrawEvaluationControlImGui()
{
#ifdef USE_IMGUI
    Object3d* controlTarget = FindEvaluationSkinningControlObject(); // 操作対象のSkinningモデル
    ImGui::Text("Move Target: %s", controlTarget ? controlTarget->GetDebugName().c_str() : "none");
    ImGui::Text("Keyboard Move: W/A/S/D");
    ImGui::Text("Pad Move: Left Stick");
    ImGui::SeparatorText("Animation");
    ImGui::Text("Keyboard: P Play/Pause, U/I Speed, O Reset");
    ImGui::Text("Pad: A Play/Pause, LB/RB Speed, X Reset");
    if (ImGui::Button("Play / Pause")) {
        ToggleSceneAnimationEnabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        ResetSceneAnimations();
    }
    if (ImGui::Button("Speed -")) {
        AdjustSceneAnimationPlaybackSpeed(-kEvaluationAnimationPlaybackSpeedStep);
    }
    ImGui::SameLine();
    if (ImGui::Button("Speed +")) {
        AdjustSceneAnimationPlaybackSpeed(kEvaluationAnimationPlaybackSpeedStep);
    }
#else
    (void)this;
#endif
}
