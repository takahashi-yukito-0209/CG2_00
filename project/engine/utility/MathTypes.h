#pragma once

namespace Math {

/// <summary>
/// 2次元ベクトルを表す構造体。
/// </summary>
struct Vector2 {
    float x, y; // X成分、Y成分
};

/// <summary>
/// 3次元ベクトルを表す構造体。
/// </summary>
struct Vector3 {
    float x, y, z; // X成分、Y成分、Z成分

    /// <summary>
    /// Vector3 同士を加算する。
    /// </summary>
    Vector3 operator+(const Vector3& rhs) const;

    /// <summary>
    /// Vector3 同士を減算する。
    /// </summary>
    Vector3 operator-(const Vector3& rhs) const;

    /// <summary>
    /// Vector3 にスカラー値を掛ける。
    /// </summary>
    Vector3 operator*(float scalar) const;

    /// <summary>
    /// Vector3 をスカラー値で割る。
    /// </summary>
    Vector3 operator/(float scalar) const;

    /// <summary>
    /// Vector3 同士を加算して代入する。
    /// </summary>
    Vector3& operator+=(const Vector3& rhs);

    /// <summary>
    /// Vector3 同士を減算して代入する。
    /// </summary>
    Vector3& operator-=(const Vector3& rhs);

    /// <summary>
    /// Vector3 にスカラー値を掛けて代入する。
    /// </summary>
    Vector3& operator*=(float scalar);

    /// <summary>
    /// Vector3 をスカラー値で割って代入する。
    /// </summary>
    Vector3& operator/=(float scalar);

    /// <summary>
    /// Vector3 の符号を反転する。
    /// </summary>
    Vector3 operator-() const;
};

/// <summary>
/// 4次元ベクトルを表す構造体。
/// </summary>
struct Vector4 {
    float x, y, z, w; // X成分、Y成分、Z成分、W成分
};

/// <summary>
/// Quaternion を表す構造体。
/// </summary>
struct Quaternion {
    float x, y, z, w; // 虚部XYZと実部W
};

/// <summary>
/// 3x3 行列を表す構造体。
/// </summary>
struct Matrix3x3 {
    float m[3][3]; // 行列要素
};

/// <summary>
/// 4x4 行列を表す構造体。
/// </summary>
struct Matrix4x4 {
    float m[4][4]; // 行列要素

    /// <summary>
    /// Matrix4x4 同士を乗算する。
    /// </summary>
    Matrix4x4 operator*(const Matrix4x4& rhs) const;
};

/// <summary>
/// Euler 角で回転を扱う座標変換情報。
/// </summary>
struct EulerTransform {
    Vector3 scale; // 拡大縮小
    Vector3 rotate; // 回転
    Vector3 translate; // 平行移動
};

/// <summary>
/// Quaternion で回転を扱う座標変換情報。
/// </summary>
struct QuaternionTransform {
    Vector3 scale; // 拡大縮小
    Quaternion rotate; // 回転
    Vector3 translate; // 平行移動
};

using Transform = EulerTransform;

} // namespace Math