#include "Player.h"

#include "../../engine/3d/Object3d.h"
#include "../../engine/io/InputManager.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

#include <algorithm>
#include <cmath>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr float kMoveInputDeadZone = 0.0001f; // 移動入力を無視する最小値
constexpr float kMaxMoveInputLength = 1.0f; // 正規化後の最大入力長
constexpr float kImGuiMoveSpeedStep = 0.05f; // 移動速度の調整幅
constexpr float kImGuiMoveSpeedMin = 0.0f; // 移動速度の最小値
constexpr float kImGuiMoveSpeedMax = 20.0f; // 移動速度の最大値
constexpr float kImGuiTransformStep = 0.01f; // Transformの調整幅
constexpr float kImGuiPhysicsStep = 0.05f; // 物理値の調整幅
constexpr float kForwardAngleOffset = 0.0f; // モデル前方と移動方向を合わせるための補正角
constexpr float kPlayerPlaneZ = 0.0f; // 2.5D操作で固定する奥行き座標
constexpr float kPlatformSnapTolerance = 0.08f; // 上面着地を許容する足元のずれ
constexpr float kPlatformHorizontalInset = 0.02f; // 端での不安定な着地を避ける内側余白
constexpr Vector3 kBaseBodyHalfSize = { 0.5f, 0.5f, 0.5f }; // 仮ブロックモデルの基準半サイズ
constexpr uint8_t kJumpKey = DIK_SPACE; // ジャンプキー

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
/// プレイヤーと足場の横方向・奥行き範囲が重なっているか判定する。
/// </summary>
bool HasHorizontalOverlap(const Vector3& playerCenter, const Vector3& playerHalfSize, const StandablePlatform& platform)
{
    const float platformHalfX = (std::max)(platform.halfSize.x - kPlatformHorizontalInset, 0.0f); // 判定に使う足場X半幅
    const float platformHalfZ = (std::max)(platform.halfSize.z - kPlatformHorizontalInset, 0.0f); // 判定に使う足場Z半幅
    const bool overlapsX = std::fabs(playerCenter.x - platform.center.x) <= playerHalfSize.x + platformHalfX; // X方向の重なり
    const bool overlapsZ = std::fabs(playerCenter.z - platform.center.z) <= playerHalfSize.z + platformHalfZ; // Z方向の重なり
    return overlapsX && overlapsZ;
}

/// <summary>
/// プレイヤーと全面コライダーが重なっているか判定する。
/// </summary>
bool HasSolidOverlap(const Vector3& playerCenter, const Vector3& playerHalfSize, const SolidCollider& collider)
{
    if (!collider.enabled) {
        return false;
    }

    const bool overlapsX = std::fabs(playerCenter.x - collider.center.x) < playerHalfSize.x + collider.halfSize.x; // X方向の重なり
    const bool overlapsY = std::fabs(playerCenter.y - collider.center.y) < playerHalfSize.y + collider.halfSize.y; // Y方向の重なり
    const bool overlapsZ = std::fabs(playerCenter.z - collider.center.z) < playerHalfSize.z + collider.halfSize.z; // Z方向の重なり
    return overlapsX && overlapsY && overlapsZ;
}

/// <summary>
/// プレイヤーと全面コライダーのX/Z範囲が重なっているか判定する。
/// </summary>
bool HasSolidHorizontalOverlap(const Vector3& playerCenter, const Vector3& playerHalfSize, const SolidCollider& collider)
{
    if (!collider.enabled) {
        return false;
    }

    const bool overlapsX = std::fabs(playerCenter.x - collider.center.x) < playerHalfSize.x + collider.halfSize.x; // X方向の重なり
    const bool overlapsZ = std::fabs(playerCenter.z - collider.center.z) < playerHalfSize.z + collider.halfSize.z; // Z方向の重なり
    return overlapsX && overlapsZ;
}
}

/// <summary>
/// デストラクタ
/// </summary>
Player::~Player() = default;

/// <summary>
/// プレイヤー表示用の3Dオブジェクトを初期化する。
/// </summary>
void Player::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const std::string& modelFileName)
{
    state_ = PlayerState {};
    initialState_ = state_;
    verticalVelocity_ = 0.0f;

    object3d_ = std::make_unique<Object3d>(); // プレイヤー表示用オブジェクト
    object3d_->Initialize(object3dCommon, imguiManager);
    object3d_->SetModel(modelFileName);
    object3d_->SetMaterialColor(materialColor_);
    object3d_->SetTranslate(state_.transform.translate);
    object3d_->SetRotate(state_.transform.rotate);
    object3d_->SetScale(state_.transform.scale);
}

/// <summary>
/// プレイヤーが保持する表示用リソースを解放する。
/// </summary>
void Player::Finalize()
{
    object3d_.reset();
    state_ = PlayerState {};
    initialState_ = state_;
    verticalVelocity_ = 0.0f;
}

/// <summary>
/// プレイヤー状態を初期状態へ戻す。
/// </summary>
void Player::Reset()
{
    state_ = initialState_;
    verticalVelocity_ = 0.0f;
}

/// <summary>
/// リセット時に戻す初期状態を設定する。
/// </summary>
void Player::SetInitialState(const PlayerState& state)
{
    initialState_ = state;
    Reset();
}

/// <summary>
/// 移動入力を取得する。
/// </summary>
Vector3 Player::BuildMoveInput() const
{
    InputManager* inputManager = InputManager::GetInstance(); // 入力状態を取得する管理クラス
    if (!inputManager) {
        return { 0.0f, 0.0f, 0.0f };
    }

    Vector3 moveInput { 0.0f, 0.0f, 0.0f }; // キーボードとパッドを合成した横移動入力
    if (inputManager->IsKeyPressed(DIK_A)) {
        moveInput.x -= 1.0f;
    }
    if (inputManager->IsKeyPressed(DIK_D)) {
        moveInput.x += 1.0f;
    }

    moveInput.x += inputManager->GetGamePadLeftStickX();
    moveInput.z = 0.0f;
    return moveInput;
}

/// <summary>
/// 入力ベクトルを移動方向として扱える長さへ正規化する。
/// </summary>
Vector3 Player::NormalizeMoveInput(const Vector3& moveInput) const
{
    const float length = std::sqrt(moveInput.x * moveInput.x + moveInput.y * moveInput.y + moveInput.z * moveInput.z); // 入力ベクトルの長さ
    if (length <= kMoveInputDeadZone) {
        return { 0.0f, 0.0f, 0.0f };
    }
    if (length <= kMaxMoveInputLength) {
        return moveInput;
    }

    return {
        moveInput.x / length,
        moveInput.y / length,
        moveInput.z / length
    };
}

/// <summary>
/// ジャンプ入力があったか判定する。
/// </summary>
bool Player::ShouldJump() const
{
    InputManager* inputManager = InputManager::GetInstance(); // ジャンプ入力を取得する管理クラス
    if (!inputManager) {
        return false;
    }

    return inputManager->IsKeyJustPressed(kJumpKey) || inputManager->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_A);
}

/// <summary>
/// 2.5D用の横移動を更新する。
/// </summary>
void Player::UpdateHorizontalMovement(float deltaTime, bool canAcceptInput, const std::vector<SolidCollider>& solidColliders)
{
    state_.isMoving = false;
    state_.transform.translate.z = kPlayerPlaneZ;
    if (!canAcceptInput) {
        return;
    }

    const Vector3 moveInput = NormalizeMoveInput(BuildMoveInput()); // 正規化した横移動入力
    if (moveInput.x == 0.0f) {
        return;
    }

    const float previousCenterX = state_.transform.translate.x; // 横移動前の中心X座標
    state_.transform.translate.x += moveInput.x * moveSpeed_ * deltaTime;
    state_.transform.translate.z = kPlayerPlaneZ;
    ResolveHorizontalSolidCollisions(previousCenterX, solidColliders);
    state_.transform.rotate.y = std::atan2(moveInput.x, 0.0f) + kForwardAngleOffset;
    state_.isMoving = true;
}

/// <summary>
/// 全面コライダーへの横方向衝突を解決する。
/// </summary>
void Player::ResolveHorizontalSolidCollisions(float previousCenterX, const std::vector<SolidCollider>& solidColliders)
{
    const Vector3 playerHalfSize = CalculateBodyHalfSize(state_.transform); // 現在のプレイヤー半サイズ
    for (const SolidCollider& collider : solidColliders) {
        if (!HasSolidOverlap(state_.transform.translate, playerHalfSize, collider)) {
            continue;
        }

        const float playerFootY = state_.transform.translate.y - playerHalfSize.y; // 現在の足元Y座標
        const float colliderTopY = collider.center.y + collider.halfSize.y; // コライダー上面Y座標
        if (playerFootY >= colliderTopY - kPlatformSnapTolerance) {
            continue;
        }

        const float colliderMinX = collider.center.x - collider.halfSize.x; // コライダー左端
        const float colliderMaxX = collider.center.x + collider.halfSize.x; // コライダー右端
        if (previousCenterX <= collider.center.x) {
            state_.transform.translate.x = colliderMinX - playerHalfSize.x;
        } else {
            state_.transform.translate.x = colliderMaxX + playerHalfSize.x;
        }
    }
}

/// <summary>
/// 全面コライダーへの縦方向衝突を解決する。
/// </summary>
bool Player::ResolveVerticalSolidCollisions(float previousFootY, float previousHeadY, const std::vector<SolidCollider>& solidColliders)
{
    const Vector3 playerHalfSize = CalculateBodyHalfSize(state_.transform); // 現在のプレイヤー半サイズ
    const float currentFootY = state_.transform.translate.y - playerHalfSize.y; // 現在の足元Y座標
    const float currentHeadY = state_.transform.translate.y + playerHalfSize.y; // 現在の頭上Y座標

    if (verticalVelocity_ <= 0.0f) {
        bool hasLandingCollider = false; // 着地候補があるか
        float landingTopY = groundY_; // 採用するコライダー上面Y座標
        for (const SolidCollider& collider : solidColliders) {
            if (!HasSolidHorizontalOverlap(state_.transform.translate, playerHalfSize, collider)) {
                continue;
            }

            const float colliderTopY = collider.center.y + collider.halfSize.y; // コライダー上面Y座標
            const bool passedThroughTop = previousFootY >= colliderTopY - kPlatformSnapTolerance && currentFootY <= colliderTopY; // 上面をまたいだか
            if (!passedThroughTop) {
                continue;
            }
            if (!hasLandingCollider || colliderTopY > landingTopY) {
                landingTopY = colliderTopY;
                hasLandingCollider = true;
            }
        }

        if (hasLandingCollider) {
            state_.transform.translate.y = landingTopY + playerHalfSize.y;
            verticalVelocity_ = 0.0f;
            state_.isGrounded = true;
            return true;
        }
    }

    if (verticalVelocity_ > 0.0f) {
        for (const SolidCollider& collider : solidColliders) {
            if (!HasSolidHorizontalOverlap(state_.transform.translate, playerHalfSize, collider)) {
                continue;
            }

            const float colliderBottomY = collider.center.y - collider.halfSize.y; // コライダー下面Y座標
            const bool passedThroughBottom = previousHeadY <= colliderBottomY + kPlatformSnapTolerance && currentHeadY >= colliderBottomY; // 下面をまたいだか
            if (!passedThroughBottom) {
                continue;
            }

            state_.transform.translate.y = colliderBottomY - playerHalfSize.y;
            verticalVelocity_ = 0.0f;
            state_.isGrounded = false;
            return true;
        }
    }

    return false;
}

/// <summary>
/// 上面足場への着地を解決する。
/// </summary>
bool Player::ResolveLandingOnPlatforms(float previousFootY, const std::vector<StandablePlatform>& standablePlatforms)
{
    if (verticalVelocity_ > 0.0f) {
        return false;
    }

    const Vector3 playerHalfSize = CalculateBodyHalfSize(state_.transform); // 現在のプレイヤー半サイズ
    const float currentFootY = state_.transform.translate.y - playerHalfSize.y; // 現在の足元Y座標
    bool hasLandingPlatform = false; // 着地候補があるか
    float landingTopY = groundY_; // 採用する足場上面Y座標

    for (const StandablePlatform& platform : standablePlatforms) {
        if (!platform.enabled || !HasHorizontalOverlap(state_.transform.translate, playerHalfSize, platform)) {
            continue;
        }

        const float platformTopY = platform.center.y + platform.halfSize.y; // 足場の上面Y座標
        const bool passedThroughTop = previousFootY >= platformTopY - kPlatformSnapTolerance && currentFootY <= platformTopY; // 上面をまたいだか
        if (!passedThroughTop) {
            continue;
        }

        if (!hasLandingPlatform || platformTopY > landingTopY) {
            landingTopY = platformTopY;
            hasLandingPlatform = true;
        }
    }

    if (!hasLandingPlatform) {
        return false;
    }

    state_.transform.translate.y = landingTopY + playerHalfSize.y;
    verticalVelocity_ = 0.0f;
    state_.isGrounded = true;
    return true;
}

/// <summary>
/// 基準床への着地を解決する。
/// </summary>
void Player::ResolveLandingOnGround()
{
    if (state_.isGrounded || verticalVelocity_ > 0.0f) {
        return;
    }

    const Vector3 playerHalfSize = CalculateBodyHalfSize(state_.transform); // 現在のプレイヤー半サイズ
    const float currentFootY = state_.transform.translate.y - playerHalfSize.y; // 現在の足元Y座標
    if (currentFootY > groundY_) {
        return;
    }

    state_.transform.translate.y = groundY_ + playerHalfSize.y;
    verticalVelocity_ = 0.0f;
    state_.isGrounded = true;
}

/// <summary>
/// ジャンプ、重力、上面着地を更新する。
/// </summary>
void Player::UpdateVerticalMovement(float deltaTime, bool canAcceptInput, const std::vector<SolidCollider>& solidColliders, const std::vector<StandablePlatform>& standablePlatforms)
{
    const Vector3 playerHalfSize = CalculateBodyHalfSize(state_.transform); // 更新前のプレイヤー半サイズ
    const float previousFootY = state_.transform.translate.y - playerHalfSize.y; // 更新前の足元Y座標
    const float previousHeadY = state_.transform.translate.y + playerHalfSize.y; // 更新前の頭上Y座標

    if (canAcceptInput && state_.isGrounded && ShouldJump()) {
        verticalVelocity_ = jumpVelocity_;
        state_.isGrounded = false;
    }

    verticalVelocity_ += gravity_ * deltaTime;
    state_.transform.translate.y += verticalVelocity_ * deltaTime;
    state_.isGrounded = false;

    if (!ResolveVerticalSolidCollisions(previousFootY, previousHeadY, solidColliders) && !ResolveLandingOnPlatforms(previousFootY, standablePlatforms)) {
        ResolveLandingOnGround();
    }
}

/// <summary>
/// 入力、全面コライダー、上面足場からプレイヤー状態を更新する。
/// </summary>
void Player::Update(float deltaTime, bool canAcceptInput, const std::vector<SolidCollider>& solidColliders, const std::vector<StandablePlatform>& standablePlatforms)
{
    if (!enabled_) {
        state_.isMoving = false;
        return;
    }

    UpdateHorizontalMovement(deltaTime, canAcceptInput, solidColliders);
    UpdateVerticalMovement(deltaTime, canAcceptInput, solidColliders, standablePlatforms);
    state_.transform.translate.z = kPlayerPlaneZ;
}

/// <summary>
/// プレイヤーを上面足場として扱うための情報を取得する。
/// </summary>
StandablePlatform Player::GetStandablePlatform() const
{
    StandablePlatform platform {}; // プレイヤーから作成する足場情報
    platform.center = state_.transform.translate;
    platform.halfSize = CalculateBodyHalfSize(state_.transform);
    platform.enabled = enabled_;
    return platform;
}

/// <summary>
/// 現在の状態を表示用Object3dへ反映する。
/// </summary>
void Player::UpdateObject(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix)
{
    if (!object3d_) {
        return;
    }

    object3d_->SetScale(state_.transform.scale);
    object3d_->SetRotate(state_.transform.rotate);
    object3d_->SetTranslate(state_.transform.translate);
    object3d_->Update(viewMatrix, projectionMatrix);
}

/// <summary>
/// プレイヤーを描画する。
/// </summary>
void Player::Draw()
{
    if (object3d_) {
        object3d_->Draw();
    }
}

/// <summary>
/// プレイヤーの表示色を設定する。
/// </summary>
void Player::SetMaterialColor(const Vector4& color)
{
    materialColor_ = color;
    if (object3d_) {
        object3d_->SetMaterialColor(materialColor_);
    }
}

/// <summary>
/// ImGuiでプレイヤー状態を表示・調整する。
/// </summary>
void Player::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Checkbox("Enabled", &enabled_);
    ImGui::Text("Move: A/D or Left Stick X");
    ImGui::Text("2.5D Plane Z: %.2f", kPlayerPlaneZ);
    ImGui::Text("Jump: Space or GamePad A");
    ImGui::Text("Moving: %s", state_.isMoving ? "true" : "false");
    ImGui::Text("Grounded: %s", state_.isGrounded ? "true" : "false");
    ImGui::Text("Vertical Velocity: %.2f", verticalVelocity_);
    ImGui::DragFloat("Move Speed", &moveSpeed_, kImGuiMoveSpeedStep, kImGuiMoveSpeedMin, kImGuiMoveSpeedMax);
    ImGui::DragFloat("Jump Velocity", &jumpVelocity_, kImGuiPhysicsStep, 0.0f, 30.0f);
    ImGui::DragFloat("Gravity", &gravity_, kImGuiPhysicsStep, -60.0f, 0.0f);
    ImGui::DragFloat("Ground Y", &groundY_, kImGuiTransformStep, -100.0f, 100.0f);
    ImGui::DragFloat3("Scale", &state_.transform.scale.x, kImGuiTransformStep, 0.001f, 100.0f);
    ImGui::DragFloat3("Rotate", &state_.transform.rotate.x, kImGuiTransformStep);
    ImGui::DragFloat3("Translate", &state_.transform.translate.x, kImGuiTransformStep);
    if (ImGui::Button("Reset Player")) {
        Reset();
    }
#endif
}