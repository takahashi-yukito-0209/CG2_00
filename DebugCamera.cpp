#include "DebugCamera.h"
#include "Input.h"

DebugCamera::DebugCamera() { }

DebugCamera::~DebugCamera() { }

void DebugCamera::Initialize(float screenWidth, float screenHeight)
{
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;

    translation_ = { 0.0f, 0.0f, -50.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };

    UpdateProjectionMatrix();
    UpdateViewMatrix();

     viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;
}

void DebugCamera::Update()
{
    Input* input = Input::GetInstance();

    // キーボード入力処理（WASD + QE）
    if (input->IsKeyPressed(DIK_W)) {
        translation_.z += moveSpeed_;
    }
    if (input->IsKeyPressed(DIK_S)) {
        translation_.z -= moveSpeed_;
    }
    if (input->IsKeyPressed(DIK_D)) {
        translation_.x -= moveSpeed_;
    }
    if (input->IsKeyPressed(DIK_A)) {
        translation_.x += moveSpeed_;
    }
    if (input->IsKeyPressed(DIK_E)) {
        translation_.y -= moveSpeed_;
    }
    if (input->IsKeyPressed(DIK_Q)) {
        translation_.y += moveSpeed_;
    }

    // 行列更新
    UpdateViewMatrix();
    viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;
}

void DebugCamera::OnMouseDrag(float deltaX, float deltaY)
{
    Input* input = Input::GetInstance();

    if (input->IsMouseButtonPressed(0)) {
        // マウス移動に応じて回転を変更
        rotation_.y += deltaX * rotateSpeed_;
        rotation_.x += deltaY * rotateSpeed_;
    }
}

void DebugCamera::OnMouseWheel(float delta)
{
    // ホイールでZ軸移動（ズーム）
    float zoomSpeed = 0.1f;
    translation_.z += delta * zoomSpeed;
}

void DebugCamera::UpdateViewMatrix()
{
    // 回転（Z→Y→X）
    Matrix4x4 rotX = math_.MakeRotateXMatrix(rotation_.x);
    Matrix4x4 rotY = math_.MakeRotateYMatrix(rotation_.y);
    Matrix4x4 rotZ = math_.MakeRotateZMatrix(rotation_.z);
    Matrix4x4 rotationMatrix = rotZ * rotY * rotX;

    // 平行移動（逆方向に移動）
    Matrix4x4 translationMatrix = math_.MakeTranslateMatrix(-translation_);

    // ビュー行列 = 回転 × 移動
    viewMatrix_ = rotationMatrix * translationMatrix;
}

void DebugCamera::UpdateProjectionMatrix()
{
    // アスペクト比
    float aspectRatio = screenWidth_ / screenHeight_;

    // パースの強さ（FOV = 画角）、アスペクト比、ニア・ファークリップ
    float fovY = 0.45f; // 縦方向の視野角（ラジアン）
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    // 透視投影行列を生成
    projectionMatrix_ = math_.MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
}
