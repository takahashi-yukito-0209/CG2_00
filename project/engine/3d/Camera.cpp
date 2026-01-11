#include "Camera.h"

using namespace MyEngine;

Camera::Camera()
    : transform_{{1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f}},
      fovY_(0.45f), aspectRatio_(16.0f/9.0f), nearClip_(0.1f), farClip_(1000.0f),
      worldMatrix_{}, viewMatrix_{}, projectionMatrix_{}, viewProjectionMatrix_{}
{
    Update();
}

void Camera::Update()
{
    worldMatrix_ = math_.MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    viewProjectionMatrix_ = math_.Multiply(viewMatrix_, projectionMatrix_);
}

void Camera::UpdateViewMatrix()
{
    Matrix4x4 rotX = math_.MakeRotateXMatrix(transform_.rotate.x);
    Matrix4x4 rotY = math_.MakeRotateYMatrix(transform_.rotate.y);
    Matrix4x4 rotZ = math_.MakeRotateZMatrix(transform_.rotate.z);
    Matrix4x4 rotationMatrix = math_.Multiply(rotZ, math_.Multiply(rotY, rotX));
    Matrix4x4 translationMatrix = math_.MakeTranslateMatrix(Vector3{-transform_.translate.x, -transform_.translate.y, -transform_.translate.z});
    viewMatrix_ = math_.Multiply(rotationMatrix, translationMatrix);
}

void Camera::UpdateProjectionMatrix()
{
    projectionMatrix_ = math_.MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
}
