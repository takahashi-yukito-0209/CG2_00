#include "Camera.h"

using namespace MyEngine;

/// <summary>
/// コンストラクタ：カメラの変換情報を初期化し、ワールド・ビュー・プロジェクション行列を計算
/// </summary>
Camera::Camera()
    : transform_{{1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f}},
      fovY_(0.45f), aspectRatio_(16.0f/9.0f), nearClip_(0.1f), farClip_(1000.0f),
      worldMatrix_{}, viewMatrix_{}, projectionMatrix_{}, viewProjectionMatrix_{}
{
    Update(); // ワールド・ビュー・プロジェクション行列の初期計算
}

/// <summary>
/// 更新処理：ワールド行列を計算し、ビュー・プロジェクション行列を更新してビュープロジェクション行列を計算
/// </summary>
void Camera::Update()
{
    // ワールド行列の計算
    worldMatrix_ = math_.MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // ビュー行列とプロジェクション行列の更新
    UpdateViewMatrix(); // ビュー行列の更新
    UpdateProjectionMatrix(); // プロジェクション行列の更新
    // ビュープロジェクション行列の計算
    viewProjectionMatrix_ = math_.Multiply(viewMatrix_, projectionMatrix_);
}

/// <summary>
/// ビュー行列の更新：回転と平行移動からビュー行列を計算
/// </summary>
void Camera::UpdateViewMatrix()
{
    // 回転行列の作成
    Matrix4x4 rotX = math_.MakeRotateXMatrix(transform_.rotate.x); // X軸回転
    Matrix4x4 rotY = math_.MakeRotateYMatrix(transform_.rotate.y); // Y軸回転
    Matrix4x4 rotZ = math_.MakeRotateZMatrix(transform_.rotate.z); // Z軸回転
    // 回転行列を合成（Z * Y * Xの順で回転）
    Matrix4x4 rotationMatrix = math_.Multiply(rotZ, math_.Multiply(rotY, rotX));
    
    // 平行移動行列の作成（カメラの位置を反転させる）
    Matrix4x4 translationMatrix = math_.MakeTranslateMatrix(Vector3{-transform_.translate.x, -transform_.translate.y, -transform_.translate.z});
    // ビュー行列の計算（回転と平行移動を合成）
    viewMatrix_ = math_.Multiply(rotationMatrix, translationMatrix);
}

/// <summary>
/// プロジェクション行列の更新：視野角、アスペクト比、近クリップ距離、遠クリップ距離から透視投影行列を計算
/// </summary>
void Camera::UpdateProjectionMatrix()
{
    // 透視投影行列の計算
    projectionMatrix_ = math_.MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
}
