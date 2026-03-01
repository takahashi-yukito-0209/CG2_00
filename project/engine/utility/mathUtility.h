#pragma once
#include "MathTypes.h"

using namespace Math;

/// <summary>
/// 数学関連のユーティリティクラス
/// </summary>
class MathUtility {
public: // メンバ関数
    /// <summary>
    /// 正規化されたベクトルを返す
    /// </summary>
    Vector3 Normalize(const Vector3& v);

    /// <summary>
    /// 平行移動行列の作成
    /// </summary>
    Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

    /// <summary>
    /// 拡大縮小行列の作成
    /// </summary>
    Matrix4x4 MakeScaleMatrix(const Vector3& scale);

    /// <summary>
    /// ベクトルを行列で変換する
    /// </summary>
    Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

    /// <summary>
    /// 行列の掛け算
    /// </summary>
    Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

    /// <summary>
    /// 行列の逆行列を返す
    /// </summary>
    Matrix4x4 Inverse(const Matrix4x4& m);

    /// <summary>
    /// 行列の転置を返す
    /// </summary>
    Matrix4x4 Transpose(const Matrix4x4& m);

    /// <summary>
    /// 単位行列を返す
    /// </summary>
    Matrix4x4 MakeIdentity4x4();

    /// <summary>
    /// X軸回転行列の作成
    /// </summary>
    Matrix4x4 MakeRotateXMatrix(float radian);

    /// <summary>
    /// Y軸回転行列の作成
    /// </summary>
    Matrix4x4 MakeRotateYMatrix(float radian);

    /// <summary>
    /// Z軸回転行列の作成
    /// </summary>
    Matrix4x4 MakeRotateZMatrix(float radian);

    /// <summary>
    /// 拡大・回転・平行移動を一度に行うアフィン変換行列の作成
    /// </summary>
    Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

    /// <summary>
    /// // 透視投影行列の作成
    /// </summary>
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

    /// <summary>
    /// 直交投影行列の作成
    /// </summary>
    Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

    /// <summary>
    /// ビューポート変換行列の作成
    /// </summary>
    Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
};
