#pragma once
#include "mathUtility.h"

/// <summary>
/// デバッグカメラ（フリー移動＋回転＋ズーム）
/// </summary>
class DebugCamera {
public:
    DebugCamera();
    ~DebugCamera();

    /// <summary>
    /// 初期化（画面サイズなどを指定）
    /// </summary>
    void Initialize(float screenWidth, float screenHeight);

    /// <summary>
    /// 毎フレーム更新（キー/マウス入力に応じて移動や回転）
    /// </summary>
    void Update();

    /// <summary>
    /// マウスドラッグで回転
    /// </summary>
    void OnMouseDrag(float deltaX, float deltaY);

    /// <summary>
    /// ホイールズーム（Z方向移動）
    /// </summary>
    void OnMouseWheel(float delta);

    /// <summary>
    /// ビュー行列取得
    /// </summary>
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

    /// <summary>
    /// 射影行列取得（正射影）
    /// </summary>
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

    /// <summary>
    /// ビュー×プロジェクション行列取得
    /// </summary>
    const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

    /// <summary>
    /// カメラの現在位置取得
    /// </summary>
    const Vector3& GetTranslation() const { return translation_; }

    /// <summary>
    /// カメラの回転角取得
    /// </summary>
    const Vector3& GetRotation() const { return rotation_; }

    /// <summary>
    /// カメラの位置を直接設定
    /// </summary>
    void SetTranslation(const Vector3& pos) { translation_ = pos; }

    /// <summary>
    /// カメラの回転を直接設定
    /// </summary>
    void SetRotation(const Vector3& rot) { rotation_ = rot; }


private:
    /// <summary>
    /// ビュー行列を更新（内部用）
    /// </summary>
    void UpdateViewMatrix();

    /// <summary>
    /// 射影行列を更新（内部用）
    /// </summary>
    void UpdateProjectionMatrix();

    // カメラ位置
    Vector3 translation_ = { 0.0f, 0.0f, -50.0f };

    // 回転
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    // 各種行列
    Matrix4x4 viewMatrix_ = {};
    Matrix4x4 projectionMatrix_ = {};
    Matrix4x4 viewProjectionMatrix_ = {};

    // 操作パラメータ
    float moveSpeed_ = 0.1f;
    float rotateSpeed_ = 0.01f;

    float screenWidth_ = 1280.0f;
    float screenHeight_ = 720.0f;

    MathUtility math_;
};
