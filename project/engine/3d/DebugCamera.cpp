#include "DebugCamera.h"
#include "InputManager.h"

using namespace Math;
using namespace MyEngine;

namespace {
constexpr Vector3 kInitialTranslation = { 0.0f, 0.0f, -10.0f }; // 初期カメラ位置
constexpr Vector3 kInitialRotation = { 0.0f, 0.0f, 0.0f }; // 初期カメラ回転
constexpr int kMouseButtonLeft = 0; // 左マウスボタン番号
constexpr float kMouseWheelZoomSpeed = 0.1f; // ホイールズーム速度
constexpr float kDefaultFovY = 0.45f; // 縦方向の視野角
constexpr float kDefaultNearZ = 0.1f; // ニアクリップ距離
constexpr float kDefaultFarZ = 1000.0f; // ファークリップ距離
} // namespace

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
    translation_ = kInitialTranslation;
    rotation_ = kInitialRotation;

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
    // カメラの移動・回転は OnMouseDrag / OnMouseWheel でクリック条件を満たした場合だけ変更する
    UpdateViewMatrix();
    // ビュープロジェクション行列の計算
    viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;
}

/// <summary>
/// マウスドラッグで回転
/// </summary>
void DebugCamera::OnMouseDrag(float deltaX, float deltaY)
{
    InputManager* input = InputManager::GetInstance(); // 入力状態を参照するためのポインタ

    // 左クリック中のドラッグだけ回転として扱う
    if (input->IsMouseButtonPressed(kMouseButtonLeft)) {
        rotation_.y += deltaX * rotateSpeed_;
        rotation_.x += deltaY * rotateSpeed_;
    }
}
/// <summary>
/// ホイールズーム（Z方向移動）
/// </summary>
void DebugCamera::OnMouseWheel(float delta)
{
    // ホイール回転だけで拡縮として扱う
    translation_.z += delta * kMouseWheelZoomSpeed;
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
    float fovY = kDefaultFovY; // 縦方向の視野角（ラジアン）
    float nearZ = kDefaultNearZ; // ニアクリップ距離
    float farZ = kDefaultFarZ; // ファークリップ距離

    // 透視投影行列を生成
    projectionMatrix_ = MathUtil::MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
}
