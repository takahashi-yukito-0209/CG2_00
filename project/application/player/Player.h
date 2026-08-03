#pragma once

#include "PlayerState.h"

#include <memory>
#include <string>
#include <vector>

namespace MyEngine {
class ImGuiManager;
class Object3d;
class Object3dCommon;
}

/// <summary>
/// プレイヤーの入力、状態、表示用Object3dを管理するクラス
/// </summary>
class Player {
public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~Player();

    /// <summary>
    /// プレイヤー表示用の3Dオブジェクトを初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const std::string& modelFileName);

    /// <summary>
    /// プレイヤーが保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// プレイヤー状態を初期状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// リセット時に戻す初期状態を設定する。
    /// </summary>
    void SetInitialState(const PlayerState& state);

    /// <summary>
    /// 入力、全面コライダー、上面足場からプレイヤー状態を更新する。
    /// </summary>
    void Update(float deltaTime, bool canAcceptInput, const std::vector<SolidCollider>& solidColliders, const std::vector<StandablePlatform>& standablePlatforms);

    /// <summary>
    /// 現在の状態を表示用Object3dへ反映する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// プレイヤーを描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// ImGuiでプレイヤー状態を表示・調整する。
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// プレイヤーの表示色を設定する。
    /// </summary>
    void SetMaterialColor(const Math::Vector4& color);

    /// <summary>
    /// 現在のプレイヤー状態を取得する。
    /// </summary>
    const PlayerState& GetState() const { return state_; }

    /// <summary>
    /// プレイヤーを上面足場として扱うための情報を取得する。
    /// </summary>
    StandablePlatform GetStandablePlatform() const;

    /// <summary>
    /// Y方向速度を取得する。
    /// </summary>
    float GetVerticalVelocity() const { return verticalVelocity_; }

    /// <summary>
    /// プレイヤー操作が有効か取得する。
    /// </summary>
    bool GetEnabled() const { return enabled_; }

private:
    /// <summary>
    /// 移動入力を取得する。
    /// </summary>
    Math::Vector3 BuildMoveInput() const;

    /// <summary>
    /// 入力ベクトルを移動方向として扱える長さへ正規化する。
    /// </summary>
    Math::Vector3 NormalizeMoveInput(const Math::Vector3& moveInput) const;

    /// <summary>
    /// ジャンプ入力があったか判定する。
    /// </summary>
    bool ShouldJump() const;

    /// <summary>
    /// 2.5D用の横移動を更新する。
    /// </summary>
    void UpdateHorizontalMovement(float deltaTime, bool canAcceptInput, const std::vector<SolidCollider>& solidColliders);

    /// <summary>
    /// ジャンプ、重力、上面着地を更新する。
    /// </summary>
    void UpdateVerticalMovement(float deltaTime, bool canAcceptInput, const std::vector<SolidCollider>& solidColliders, const std::vector<StandablePlatform>& standablePlatforms);

    /// <summary>
    /// 全面コライダーへの横方向衝突を解決する。
    /// </summary>
    void ResolveHorizontalSolidCollisions(float previousCenterX, const std::vector<SolidCollider>& solidColliders);

    /// <summary>
    /// 全面コライダーへの縦方向衝突を解決する。
    /// </summary>
    bool ResolveVerticalSolidCollisions(float previousFootY, float previousHeadY, const std::vector<SolidCollider>& solidColliders);

    /// <summary>
    /// 上面足場への着地を解決する。
    /// </summary>
    bool ResolveLandingOnPlatforms(float previousFootY, const std::vector<StandablePlatform>& standablePlatforms);

    /// <summary>
    /// 基準床への着地を解決する。
    /// </summary>
    void ResolveLandingOnGround();

    std::unique_ptr<MyEngine::Object3d> object3d_; // 表示に使用する3Dオブジェクト
    Math::Vector4 materialColor_ { 0.35f, 0.8f, 1.0f, 1.0f }; // プレイヤーの表示色
    PlayerState state_; // 現在のプレイヤー状態
    PlayerState initialState_; // リセット時に戻す初期状態
    float moveSpeed_ = 3.0f; // 1秒あたりの移動速度
    float verticalVelocity_ = 0.0f; // Y方向の現在速度
    float jumpVelocity_ = 6.5f; // ジャンプ開始時のY方向速度
    float gravity_ = -18.0f; // Y方向の重力加速度
    float groundY_ = -8.0f; // 仮ステージ外へ落ちた時だけ使う基準床上面Y座標
    bool enabled_ = true; // プレイヤー更新を行うか
};