#include "DebugCamera.h"
#include "InputManager.h"

using namespace Math;
using namespace MyEngine;

/// <summary>
/// デフォルトコンストラクタ
/// </summary>
DebugCamera::DebugCamera() { }

/// <summary>
/// デストラクタ
/// </summary>
DebugCamera::~DebugCamera() { }

/// <summary>
/// 初期化
/// </summary>
void DebugCamera::Initialize(float screenWidth, float screenHeight)
{
    // 画面サイズを保存
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;

    // カメラ位置と回転の初期値
    translation_ = { 0.0f, 0.0f, -10.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };

    // 行列の初期化
    UpdateProjectionMatrix();
    // ビュー行列は初期位置・回転に基づいて更新
    UpdateViewMatrix();

    // ビュープロジェクション行列の計算
    viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;
}

/// <summary>
/// 更新（キー/マウス入力に応じて移動や回転）
/// </summary>
void DebugCamera::Update()
{
    InputManager* input = InputManager::GetInstance(); // 入力マネージャのインスタンスを取得

    // キーボード入力処理（WASD + QE）
    if (input->IsKeyPressed(DIK_W)) {
        translation_.z += moveSpeed_; // 前方に移動
    }
    if (input->IsKeyPressed(DIK_S)) {
        translation_.z -= moveSpeed_; // 後方に移動
    }
    if (input->IsKeyPressed(DIK_D)) {
        translation_.x -= moveSpeed_; // 右に移動
    }
    if (input->IsKeyPressed(DIK_A)) {
        translation_.x += moveSpeed_; // 左に移動
    }
    if (input->IsKeyPressed(DIK_E)) {
        translation_.y -= moveSpeed_; // 下に移動
    }
    if (input->IsKeyPressed(DIK_Q)) {
        translation_.y += moveSpeed_; // 上に移動
    }

    // 行列更新
    UpdateViewMatrix();
    // ビュープロジェクション行列の計算
    viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;
}

/// <summary>
/// マウスドラッグで回転
/// </summary>
void DebugCamera::OnMouseDrag(float deltaX, float deltaY)
{
    InputManager* input = InputManager::GetInstance(); // 入力マネージャのインスタンスを取得

    // 左クリック・右クリック・中クリックのいずれかが押されている間だけ回転を変更
    if (input->IsMouseButtonPressed(0) || input->IsMouseButtonPressed(1) || input->IsMouseButtonPressed(2)) {
        // マウス移動に応じて回転を変更
        rotation_.y += deltaX * rotateSpeed_;
        rotation_.x += deltaY * rotateSpeed_;
    }
}

/// <summary>
/// ホイールズーム（Z方向移動）
/// </summary>
void DebugCamera::OnMouseWheel(float delta)
{
    // ホイールでZ軸移動（ズーム）
    float zoomSpeed = 0.1f;
    translation_.z += delta * zoomSpeed;
}

/// <summary>
/// ビュー行列を更新（内部用）
/// </summary>
void DebugCamera::UpdateViewMatrix()
{
    // 回転行列の作成
    Matrix4x4 rotX = MathUtil::MakeRotateXMatrix(rotation_.x); // X軸回転
    Matrix4x4 rotY = MathUtil::MakeRotateYMatrix(rotation_.y); // Y軸回転
    Matrix4x4 rotZ = MathUtil::MakeRotateZMatrix(rotation_.z); // Z軸回転
    // 回転行列を合成（Z→Y→Xの順で回転）
    Matrix4x4 rotationMatrix = rotZ * rotY * rotX;

    // 平行移動（逆方向に移動）
    Matrix4x4 translationMatrix = MathUtil::MakeTranslateMatrix(-translation_);

    // ビュー行列 = 回転 × 移動
    viewMatrix_ = rotationMatrix * translationMatrix;
}

/// <summary>
/// 射影行列を更新（内部用）
/// </summary>
void DebugCamera::UpdateProjectionMatrix()
{
    // アスペクト比
    float aspectRatio = screenWidth_ / screenHeight_;

    // パースの強さ（FOV = 画角）、アスペクト比、ニア・ファークリップ
    float fovY = 0.45f; // 縦方向の視野角（ラジアン）
    float nearZ = 0.1f; // ニアクリップ距離
    float farZ = 1000.0f; // ファークリップ距離

    // 透視投影行列を生成
    projectionMatrix_ = MathUtil::MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
}
