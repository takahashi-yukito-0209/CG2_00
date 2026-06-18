#pragma once
#include "mathUtility.h"
#include <MathTypes.h>

namespace MyEngine {

/// <summary>
/// カメラクラス
/// </summary>
class Camera {
public:
    Camera(); // コンストラクタ

    /// <summary>
    /// 更新処理（ワールド・ビュー・プロジェクション行列の計算）
    /// </summary>
    void Update();

    /// <summary>
    /// ワールド行列の取得
    /// </summary>
    const Math::Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    /// <summary>
    /// ビュー行列を取得
    /// </summary>
    const Math::Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

    /// <summary>
    /// プロジェクション行列を取得
    /// </summary>
    const Math::Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

    /// <summary>
    /// ビュープロジェクション行列を取得
    /// </summary>
    const Math::Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

    /// <summary>
    /// 回転角（ラジアン）を取得
    /// </summary>
    const Math::Vector3& GetRotate() const { return transform_.rotate; }

    /// <summary>
    /// 座標（平行移動）を取得
    /// </summary>
    const Math::Vector3& GetTranslate() const { return transform_.translate; }

    /// <summary>
    /// 回転角（ラジアン）をセット
    /// </summary>
    void SetRotate(const Math::Vector3& rotate) { transform_.rotate = rotate; }

    /// <summary>
    /// 座標（平行移動）をセット
    /// </summary>
    void SetTranslate(const Math::Vector3& translate) { transform_.translate = translate; }

    /// <summary>
    /// 視野角（ラジアン）をセット
    /// </summary>
    void SetFovY(float fovY) { fovY_ = fovY; }

    /// <summary>
    /// アスペクト比をセット
    /// </summary>
    void SetAspectRatio(float aspect) { aspectRatio_ = aspect; }

    /// <summary>
    /// 近クリップ距離をセット
    /// </summary>
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }

    /// <summary>
    /// 遠クリップ距離をセット
    /// </summary>
    void SetFarClip(float farClip) { farClip_ = farClip; }

private: // メンバ関数
    void UpdateViewMatrix(); // ビュー行列の更新
    void UpdateProjectionMatrix(); // プロジェクション行列の更新

private: // メンバ変数
    // カメラの変換情報（回転と平行移動）
    Math::Transform transform_;

    // 視野角（ラジアン）
    float fovY_;

    // アスペクト比
    float aspectRatio_;

    // 近クリップ距離
    float nearClip_;

    // 遠クリップ距離
    float farClip_;

    // ワールド行列
    Math::Matrix4x4 worldMatrix_;

    // ビュー行列
    Math::Matrix4x4 viewMatrix_;

    // プロジェクション行列
    Math::Matrix4x4 projectionMatrix_;

    // ビュープロジェクション行列
    Math::Matrix4x4 viewProjectionMatrix_;
};

} // namespace MyEngine
