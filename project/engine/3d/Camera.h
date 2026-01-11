#pragma once
#include <MathTypes.h>
#include "mathUtility.h"

using namespace Math;

namespace MyEngine {

class Camera {
public:
    Camera();

    void Update();

    // getter
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
    const Vector3& GetRotate() const { return transform_.rotate; }
    const Vector3& GetTranslate() const { return transform_.translate; }

    // setter
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    void SetFovY(float fovY) { fovY_ = fovY; }
    void SetAspectRatio(float aspect) { aspectRatio_ = aspect; }
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) { farClip_ = farClip; }

private:
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();

private:
    Transform transform_;
    float fovY_;
    float aspectRatio_;
    float nearClip_;
    float farClip_;
    Matrix4x4 worldMatrix_;
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
    Matrix4x4 viewProjectionMatrix_;
    MathUtility math_;
};

} // namespace MyEngine
