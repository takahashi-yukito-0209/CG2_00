#pragma once
#include "MathTypes.h"

/// <summary>
/// 数学ユーティリティ関数をまとめた名前空間
/// </summary>
namespace MathUtil {
    Math::Vector3 Normalize(const Math::Vector3& v);
    Math::Matrix4x4 MakeTranslateMatrix(const Math::Vector3& translate);
    Math::Matrix4x4 MakeScaleMatrix(const Math::Vector3& scale);
    Math::Vector3 Transform(const Math::Vector3& vector, const Math::Matrix4x4& matrix);
    Math::Matrix4x4 Multiply(const Math::Matrix4x4& m1, const Math::Matrix4x4& m2);
    Math::Matrix4x4 Inverse(const Math::Matrix4x4& m);
    Math::Matrix4x4 Transpose(const Math::Matrix4x4& m);
    Math::Matrix4x4 MakeIdentity4x4();
    Math::Matrix4x4 MakeRotateXMatrix(float radian);
    Math::Matrix4x4 MakeRotateYMatrix(float radian);
    Math::Matrix4x4 MakeRotateZMatrix(float radian);
    Math::Matrix4x4 MakeAffineMatrix(const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& translate);
    Math::Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
    Math::Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
    Math::Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
}

/// 既存コード互換のための簡易なラッパクラス
class MathUtility {
public:
    Math::Vector3 Normalize(const Math::Vector3& v) { return MathUtil::Normalize(v); }
    Math::Matrix4x4 MakeTranslateMatrix(const Math::Vector3& translate) { return MathUtil::MakeTranslateMatrix(translate); }
    Math::Matrix4x4 MakeScaleMatrix(const Math::Vector3& scale) { return MathUtil::MakeScaleMatrix(scale); }
    Math::Vector3 Transform(const Math::Vector3& vector, const Math::Matrix4x4& matrix) { return MathUtil::Transform(vector, matrix); }
    Math::Matrix4x4 Multiply(const Math::Matrix4x4& m1, const Math::Matrix4x4& m2) { return MathUtil::Multiply(m1, m2); }
    Math::Matrix4x4 Inverse(const Math::Matrix4x4& m) { return MathUtil::Inverse(m); }
    Math::Matrix4x4 Transpose(const Math::Matrix4x4& m) { return MathUtil::Transpose(m); }
    Math::Matrix4x4 MakeIdentity4x4() { return MathUtil::MakeIdentity4x4(); }
    Math::Matrix4x4 MakeRotateXMatrix(float radian) { return MathUtil::MakeRotateXMatrix(radian); }
    Math::Matrix4x4 MakeRotateYMatrix(float radian) { return MathUtil::MakeRotateYMatrix(radian); }
    Math::Matrix4x4 MakeRotateZMatrix(float radian) { return MathUtil::MakeRotateZMatrix(radian); }
    Math::Matrix4x4 MakeAffineMatrix(const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& translate) { return MathUtil::MakeAffineMatrix(scale, rotate, translate); }
    Math::Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) { return MathUtil::MakePerspectiveFovMatrix(fovY, aspectRatio, nearClip, farClip); }
    Math::Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) { return MathUtil::MakeOrthographicMatrix(left, top, right, bottom, nearClip, farClip); }
    Math::Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) { return MathUtil::MakeViewportMatrix(left, top, width, height, minDepth, maxDepth); }
};
