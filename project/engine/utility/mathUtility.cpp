#include "mathUtility.h"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <numbers>

using namespace Math;

namespace {
constexpr float kNormalizeEpsilon = 0.000001f; // 正規化でゼロ長とみなすしきい値
}

/// <summary>
/// 値を 0.0f から 1.0f の範囲に収める。
/// </summary>
float MathUtil::Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

/// <summary>
/// 度数法の角度をラジアンへ変換する。
/// </summary>
float MathUtil::DegToRad(float degree)
{
    return degree * std::numbers::pi_v<float> / 180.0f;
}

/// <summary>
/// ラジアンの角度を度数法へ変換する。
/// </summary>
float MathUtil::RadToDeg(float radian)
{
    return radian * 180.0f / std::numbers::pi_v<float>;
}

/// <summary>
/// Vector3 同士の内積を計算する。
/// </summary>
float MathUtil::Dot(const Math::Vector3& a, const Math::Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// <summary>
/// Vector3 同士の外積を計算する。
/// </summary>
Math::Vector3 MathUtil::Cross(const Math::Vector3& a, const Math::Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

/// <summary>
/// Vector3 の長さの二乗を計算する。
/// </summary>
float MathUtil::LengthSquared(const Math::Vector3& v)
{
    return Dot(v, v);
}

/// <summary>
/// Vector3 の長さを計算する。
/// </summary>
float MathUtil::Length(const Math::Vector3& v)
{
    return std::sqrt(LengthSquared(v));
}

/// <summary>
/// 2点間の距離の二乗を計算する。
/// </summary>
float MathUtil::DistanceSquared(const Math::Vector3& a, const Math::Vector3& b)
{
    return LengthSquared(a - b);
}

/// <summary>
/// 2点間の距離を計算する。
/// </summary>
float MathUtil::Distance(const Math::Vector3& a, const Math::Vector3& b)
{
    return std::sqrt(DistanceSquared(a, b));
}

/// <summary>
/// ベクトルを正規化する。
/// </summary>
Math::Vector3 MathUtil::Normalize(const Math::Vector3& v)
{
    return SafeNormalize(v);
}

/// <summary>
/// 長さがほぼ 0 の場合は代替方向を返してベクトルを正規化する。
/// </summary>
Math::Vector3 MathUtil::SafeNormalize(const Math::Vector3& v, const Math::Vector3& fallback)
{
    const float length = Length(v); // ベクトルの長さ
    if (length > kNormalizeEpsilon) {
        return v / length;
    }
    return fallback;
}
Math::Matrix4x4 MathUtil::Transpose(const Math::Matrix4x4& m)
{
    Math::Matrix4x4 r = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = m.m[j][i];
        }
    }
    return r;
}

Math::Matrix4x4 MathUtil::MakeTranslateMatrix(const Math::Vector3& translate)
{
    Math::Matrix4x4 result = {};
    result = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        translate.x, translate.y, translate.z, 1.0f
    };
    return result;
}

Math::Matrix4x4 MathUtil::MakeScaleMatrix(const Math::Vector3& scale)
{
    Math::Matrix4x4 result = {};
    result = {
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return result;
}

Math::Vector3 MathUtil::Transform(const Math::Vector3& vector, const Math::Matrix4x4& matrix)
{
    Math::Vector3 result;
    result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
    result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
    result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
    float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];
    assert(w != 0.0f);
    result.x /= w;
    result.y /= w;
    result.z /= w;
    return result;
}

Math::Matrix4x4 MathUtil::Multiply(const Math::Matrix4x4& m1, const Math::Matrix4x4& m2)
{
    Math::Matrix4x4 result = {};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.m[row][col] = 0.0f;
            for (int k = 0; k < 4; ++k)
                result.m[row][col] += m1.m[row][k] * m2.m[k][col];
        }
    }
    return result;
}

Math::Matrix4x4 MathUtil::Inverse(const Math::Matrix4x4& m)
{
    Math::Matrix4x4 result = {};

    float det = m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2] - m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2] + m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2] - m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0] + m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0];
    if (det == 0.0f)
        return result;
    float invDet = 1.0f / det;
    result.m[0][0] = invDet * (m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2]);
    result.m[0][1] = invDet * (-m.m[0][1] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2] + m.m[0][3] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2]);
    result.m[0][2] = invDet * (m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[3][2] - m.m[0][3] * m.m[1][2] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2]);
    result.m[0][3] = invDet * (-m.m[0][1] * m.m[1][2] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2] + m.m[0][3] * m.m[1][2] * m.m[2][1] + m.m[0][2] * m.m[1][1] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2]);
    result.m[1][0] = invDet * (-m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2] + m.m[1][3] * m.m[2][2] * m.m[3][0] + m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2]);
    result.m[1][1] = invDet * (m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] + m.m[0][3] * m.m[2][0] * m.m[3][2] - m.m[0][3] * m.m[2][2] * m.m[3][0] - m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2]);
    result.m[1][2] = invDet * (-m.m[0][0] * m.m[1][2] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2] + m.m[0][3] * m.m[1][2] * m.m[3][0] + m.m[0][2] * m.m[1][0] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2]);
    result.m[1][3] = invDet * (m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] + m.m[0][3] * m.m[1][0] * m.m[2][2] - m.m[0][3] * m.m[1][2] * m.m[2][0] - m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2]);
    result.m[2][0] = invDet * (m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[1][3] * m.m[2][0] * m.m[3][1] - m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1]);
    result.m[2][1] = invDet * (-m.m[0][0] * m.m[2][1] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[2][1] * m.m[3][0] + m.m[0][1] * m.m[2][0] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1]);
    result.m[2][2] = invDet * (m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] + m.m[0][3] * m.m[1][0] * m.m[3][1] - m.m[0][3] * m.m[1][1] * m.m[3][0] - m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1]);
    result.m[2][3] = invDet * (-m.m[0][0] * m.m[1][1] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] + m.m[0][1] * m.m[1][0] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1]);
    result.m[3][0] = invDet * (-m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][2] * m.m[2][0] * m.m[3][1] + m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1]);
    result.m[3][1] = invDet * (m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] + m.m[0][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[2][1] * m.m[3][0] - m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1]);
    result.m[3][2] = invDet * (-m.m[0][0] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1] + m.m[0][2] * m.m[1][1] * m.m[3][0] + m.m[0][1] * m.m[1][0] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1]);
    result.m[3][3] = invDet * (m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] + m.m[0][2] * m.m[1][0] * m.m[2][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] - m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1]);
    return result;
}

Math::Matrix4x4 MathUtil::MakeIdentity4x4()
{
    Math::Matrix4x4 result = {};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col)
            result.m[row][col] = (row == col) ? 1.0f : 0.0f;
    }
    return result;
}

Math::Matrix4x4 MathUtil::MakeRotateXMatrix(float radian)
{
    Math::Matrix4x4 result = {};
    result = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, std::cos(radian), std::sin(radian), 0.0f, 0.0f, std::sin(-radian), std::cos(radian), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}

Math::Matrix4x4 MathUtil::MakeRotateYMatrix(float radian)
{
    Math::Matrix4x4 result = {};
    result = { std::cos(radian), 0.0f, std::sin(-radian), 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, std::sin(radian), 0.0f, std::cos(radian), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}

Math::Matrix4x4 MathUtil::MakeRotateZMatrix(float radian)
{
    Math::Matrix4x4 result = {};
    result = { std::cos(radian), std::sin(radian), 0.0f, 0.0f, std::sin(-radian), std::cos(radian), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}

Math::Matrix4x4 MathUtil::MakeAffineMatrix(const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& translate)
{
    Math::Matrix4x4 rotateXYZMatrix = Multiply(MakeRotateXMatrix(rotate.x), Multiply(MakeRotateYMatrix(rotate.y), MakeRotateZMatrix(rotate.z)));
    Math::Matrix4x4 result = Multiply(Multiply(MakeScaleMatrix(scale), rotateXYZMatrix), MakeTranslateMatrix(translate));
    return result;
}

/// <summary>
/// 2つのVector3を線形補間する
/// </summary>
Math::Vector3 MathUtil::Lerp(const Math::Vector3& start, const Math::Vector3& end, float t)
{
    const float clampedT = std::clamp(t, 0.0f, 1.0f); // 補間率
    return {
        start.x + (end.x - start.x) * clampedT,
        start.y + (end.y - start.y) * clampedT,
        start.z + (end.z - start.z) * clampedT
    };
}

/// <summary>
/// 2つのQuaternionを球面線形補間する
/// </summary>
Math::Quaternion MathUtil::Slerp(const Math::Quaternion& start, const Math::Quaternion& end, float t)
{
    const float clampedT = std::clamp(t, 0.0f, 1.0f); // 補間率
    Math::Quaternion correctedEnd = end; // 最短経路に補正した終点
    float dot = start.x * end.x + start.y * end.y + start.z * end.z + start.w * end.w; // Quaternion同士の内積

    if (dot < 0.0f) {
        correctedEnd = { -end.x, -end.y, -end.z, -end.w };
        dot = -dot;
    }

    constexpr float kLinearThreshold = 0.9995f; // 線形補間へ切り替える近似しきい値
    if (dot > kLinearThreshold) {
        Math::Quaternion result = {
            start.x + (correctedEnd.x - start.x) * clampedT,
            start.y + (correctedEnd.y - start.y) * clampedT,
            start.z + (correctedEnd.z - start.z) * clampedT,
            start.w + (correctedEnd.w - start.w) * clampedT
        }; // 線形補間したQuaternion
        const float length = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w); // 正規化用の長さ
        if (length > 0.000001f) {
            result.x /= length;
            result.y /= length;
            result.z /= length;
            result.w /= length;
        }
        return result;
    }

    dot = std::clamp(dot, -1.0f, 1.0f);
    const float theta = std::acos(dot); // Quaternion間の角度
    const float sinTheta = std::sin(theta); // 球面補間の分母
    const float startScale = std::sin((1.0f - clampedT) * theta) / sinTheta; // 始点側の重み
    const float endScale = std::sin(clampedT * theta) / sinTheta; // 終点側の重み

    return {
        start.x * startScale + correctedEnd.x * endScale,
        start.y * startScale + correctedEnd.y * endScale,
        start.z * startScale + correctedEnd.z * endScale,
        start.w * startScale + correctedEnd.w * endScale
    };
}

/// <summary>
/// Quaternionから回転行列を作成する
/// </summary>
Math::Matrix4x4 MathUtil::MakeRotateMatrix(const Math::Quaternion& rotate)
{
    const float xx = rotate.x * rotate.x; // x成分の二乗
    const float yy = rotate.y * rotate.y; // y成分の二乗
    const float zz = rotate.z * rotate.z; // z成分の二乗
    const float xy = rotate.x * rotate.y; // xy成分
    const float xz = rotate.x * rotate.z; // xz成分
    const float yz = rotate.y * rotate.z; // yz成分
    const float wx = rotate.w * rotate.x; // wx成分
    const float wy = rotate.w * rotate.y; // wy成分
    const float wz = rotate.w * rotate.z; // wz成分

    Math::Matrix4x4 result = MakeIdentity4x4(); // 作成する回転行列
    result.m[0][0] = 1.0f - 2.0f * (yy + zz);
    result.m[0][1] = 2.0f * (xy + wz);
    result.m[0][2] = 2.0f * (xz - wy);
    result.m[1][0] = 2.0f * (xy - wz);
    result.m[1][1] = 1.0f - 2.0f * (xx + zz);
    result.m[1][2] = 2.0f * (yz + wx);
    result.m[2][0] = 2.0f * (xz + wy);
    result.m[2][1] = 2.0f * (yz - wx);
    result.m[2][2] = 1.0f - 2.0f * (xx + yy);
    return result;
}

/// <summary>
/// scale、Quaternion回転、translateからアフィン行列を作成する
/// </summary>
Math::Matrix4x4 MathUtil::MakeAffineMatrix(const Math::Vector3& scale, const Math::Quaternion& rotate, const Math::Vector3& translate)
{
    return Multiply(Multiply(MakeScaleMatrix(scale), MakeRotateMatrix(rotate)), MakeTranslateMatrix(translate));
}
Math::Matrix4x4 MathUtil::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip)
{
    Math::Matrix4x4 result = {};
    result.m[0][0] = 1.0f / aspectRatio * (1.0f / tanf(fovY / 2.0f));
    result.m[0][1] = 0.0f;
    result.m[0][2] = 0.0f;
    result.m[0][3] = 0.0f;
    result.m[1][0] = 0.0f;
    result.m[1][1] = 1.0f / tanf(fovY / 2.0f);
    result.m[1][2] = 0.0f;
    result.m[1][3] = 0.0f;
    result.m[2][0] = 0.0f;
    result.m[2][1] = 0.0f;
    result.m[2][2] = farClip / (farClip - nearClip);
    result.m[2][3] = 1.0f;
    result.m[3][0] = 0.0f;
    result.m[3][1] = 0.0f;
    result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
    result.m[3][3] = 0.0f;
    return result;
}

/// <summary>
/// 直交投影行列の作成
/// </summary>
Math::Matrix4x4 MathUtil::MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip)
{
    Matrix4x4 result = {};

    result.m[0][0] = 2.0f / (right - left);
    result.m[0][1] = 0.0f;
    result.m[0][2] = 0.0f;
    result.m[0][3] = 0.0f;
    result.m[1][0] = 0.0f;
    result.m[1][1] = 2.0f / (top - bottom);
    result.m[1][2] = 0.0f;
    result.m[1][3] = 0.0f;
    result.m[2][0] = 0.0f;
    result.m[2][1] = 0.0f;
    result.m[2][2] = 1.0f / (farClip - nearClip);
    result.m[2][3] = 0.0f;
    result.m[3][0] = (left + right) / (left - right);
    result.m[3][1] = (top + bottom) / (bottom - top);
    result.m[3][2] = nearClip / (nearClip - farClip);
    result.m[3][3] = 1.0f;

    return result;
}

/// <summary>
/// ビューポート変換行列の作成
/// </summary>
Math::Matrix4x4 MathUtil::MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth)
{
    Matrix4x4 result = {};

    result.m[0][0] = width / 2.0f;
    result.m[0][1] = 0.0f;
    result.m[0][2] = 0.0f;
    result.m[0][3] = 0.0f;
    result.m[1][0] = 0.0f;
    result.m[1][1] = -(height / 2.0f);
    result.m[1][2] = 0.0f;
    result.m[1][3] = 0.0f;
    result.m[2][0] = 0.0f;
    result.m[2][1] = 0.0f;
    result.m[2][2] = maxDepth - minDepth;
    result.m[2][3] = 0.0f;
    result.m[3][0] = left + (width / 2.0f);
    result.m[3][1] = top + (height / 2.0f);
    result.m[3][2] = minDepth;
    result.m[3][3] = 1.0f;

    return result;
}
