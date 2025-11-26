#pragma once

namespace Math {

// ベクトル構造体
struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;

    // 宣言
    Vector3 operator-(const Vector3 rhs) const;
    Vector3 operator-() const;
};

struct Vector4 {
    float x, y, z, w;
};

// 行列構造体
struct Matrix3x3 {
    float m[3][3];
};

struct Matrix4x4 {
    float m[4][4];

    // 宣言
    Matrix4x4 operator*(const Matrix4x4& rhs) const;
};

// 座標変換構造体
struct Transform {
    Vector3 scale, rotate, translate;
};

} // namespace Math