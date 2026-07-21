#pragma once
#include "MathTypes.h"

/// <summary>
/// 数学ユーティリティ関数をまとめた名前空間。
/// </summary>
namespace MathUtil {

/// <summary>
/// 値を 0.0f から 1.0f の範囲に収める。
/// </summary>
float Clamp01(float value);

/// <summary>
/// 度数法の角度をラジアンへ変換する。
/// </summary>
float DegToRad(float degree);

/// <summary>
/// ラジアンの角度を度数法へ変換する。
/// </summary>
float RadToDeg(float radian);

/// <summary>
/// Vector3 同士の内積を計算する。
/// </summary>
float Dot(const Math::Vector3& a, const Math::Vector3& b);

/// <summary>
/// Vector3 同士の外積を計算する。
/// </summary>
Math::Vector3 Cross(const Math::Vector3& a, const Math::Vector3& b);

/// <summary>
/// Vector3 の長さの二乗を計算する。
/// </summary>
float LengthSquared(const Math::Vector3& v);

/// <summary>
/// Vector3 の長さを計算する。
/// </summary>
float Length(const Math::Vector3& v);

/// <summary>
/// 2点間の距離の二乗を計算する。
/// </summary>
float DistanceSquared(const Math::Vector3& a, const Math::Vector3& b);

/// <summary>
/// 2点間の距離を計算する。
/// </summary>
float Distance(const Math::Vector3& a, const Math::Vector3& b);

/// <summary>
/// ベクトルを正規化する。
/// </summary>
Math::Vector3 Normalize(const Math::Vector3& v);

/// <summary>
/// 長さがほぼ 0 の場合は代替方向を返してベクトルを正規化する。
/// </summary>
Math::Vector3 SafeNormalize(const Math::Vector3& v, const Math::Vector3& fallback = { 0.0f, -1.0f, 0.0f });

/// <summary>
/// 平行移動行列を作成する。
/// </summary>
Math::Matrix4x4 MakeTranslateMatrix(const Math::Vector3& translate);

/// <summary>
/// 拡大縮小行列を作成する。
/// </summary>
Math::Matrix4x4 MakeScaleMatrix(const Math::Vector3& scale);

/// <summary>
/// ベクトルを行列で変換する。
/// </summary>
Math::Vector3 Transform(const Math::Vector3& vector, const Math::Matrix4x4& matrix);

/// <summary>
/// 4x4 行列同士を乗算する。
/// </summary>
Math::Matrix4x4 Multiply(const Math::Matrix4x4& m1, const Math::Matrix4x4& m2);

/// <summary>
/// 4x4 行列の逆行列を計算する。
/// </summary>
Math::Matrix4x4 Inverse(const Math::Matrix4x4& m);

/// <summary>
/// 4x4 行列の転置行列を計算する。
/// </summary>
Math::Matrix4x4 Transpose(const Math::Matrix4x4& m);

/// <summary>
/// 単位行列を作成する。
/// </summary>
Math::Matrix4x4 MakeIdentity4x4();

/// <summary>
/// X 軸回転行列を作成する。
/// </summary>
Math::Matrix4x4 MakeRotateXMatrix(float radian);

/// <summary>
/// Y 軸回転行列を作成する。
/// </summary>
Math::Matrix4x4 MakeRotateYMatrix(float radian);

/// <summary>
/// Z 軸回転行列を作成する。
/// </summary>
Math::Matrix4x4 MakeRotateZMatrix(float radian);

/// <summary>
/// scale、Euler 回転、translate からアフィン変換行列を作成する。
/// </summary>
Math::Matrix4x4 MakeAffineMatrix(const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& translate);

/// <summary>
/// 2つの Vector3 を線形補間する。
/// </summary>
Math::Vector3 Lerp(const Math::Vector3& start, const Math::Vector3& end, float t);

/// <summary>
/// 2つの Quaternion を球面線形補間する。
/// </summary>
Math::Quaternion Slerp(const Math::Quaternion& start, const Math::Quaternion& end, float t);

/// <summary>
/// Quaternion から回転行列を作成する。
/// </summary>
Math::Matrix4x4 MakeRotateMatrix(const Math::Quaternion& rotate);

/// <summary>
/// scale、Quaternion 回転、translate からアフィン変換行列を作成する。
/// </summary>
Math::Matrix4x4 MakeAffineMatrix(const Math::Vector3& scale, const Math::Quaternion& rotate, const Math::Vector3& translate);

/// <summary>
/// 透視投影行列を作成する。
/// </summary>
Math::Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

/// <summary>
/// 正射影行列を作成する。
/// </summary>
Math::Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

/// <summary>
/// ビューポート変換行列を作成する。
/// </summary>
Math::Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

} // namespace MathUtil

/// <summary>
/// 既存コード互換のための簡易ラッパークラス。
/// </summary>
class MathUtility {
public:
    float Clamp01(float value) { return MathUtil::Clamp01(value); }
    float DegToRad(float degree) { return MathUtil::DegToRad(degree); }
    float RadToDeg(float radian) { return MathUtil::RadToDeg(radian); }
    float Dot(const Math::Vector3& a, const Math::Vector3& b) { return MathUtil::Dot(a, b); }
    Math::Vector3 Cross(const Math::Vector3& a, const Math::Vector3& b) { return MathUtil::Cross(a, b); }
    float LengthSquared(const Math::Vector3& v) { return MathUtil::LengthSquared(v); }
    float Length(const Math::Vector3& v) { return MathUtil::Length(v); }
    float DistanceSquared(const Math::Vector3& a, const Math::Vector3& b) { return MathUtil::DistanceSquared(a, b); }
    float Distance(const Math::Vector3& a, const Math::Vector3& b) { return MathUtil::Distance(a, b); }
    Math::Vector3 Normalize(const Math::Vector3& v) { return MathUtil::Normalize(v); }
    Math::Vector3 SafeNormalize(const Math::Vector3& v, const Math::Vector3& fallback = { 0.0f, -1.0f, 0.0f }) { return MathUtil::SafeNormalize(v, fallback); }
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
    Math::Vector3 Lerp(const Math::Vector3& start, const Math::Vector3& end, float t) { return MathUtil::Lerp(start, end, t); }
    Math::Quaternion Slerp(const Math::Quaternion& start, const Math::Quaternion& end, float t) { return MathUtil::Slerp(start, end, t); }
    Math::Matrix4x4 MakeRotateMatrix(const Math::Quaternion& rotate) { return MathUtil::MakeRotateMatrix(rotate); }
    Math::Matrix4x4 MakeAffineMatrix(const Math::Vector3& scale, const Math::Quaternion& rotate, const Math::Vector3& translate) { return MathUtil::MakeAffineMatrix(scale, rotate, translate); }
};