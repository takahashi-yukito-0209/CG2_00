#pragma once

struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;

    // ベクトル同士の減算
    Vector3 operator-(const Vector3 rhs) const
    {
        return {
            x - rhs.x,
            y - rhs.y,
            z - rhs.z
        };
    }

    // 単項マイナス（符号反転）演算子
    Vector3 operator-() const
    {
        return {
            -x,
            -y,
            -z
        };
    }
};


struct Vector4 {
    float x, y, z, w;
};

struct Matrix3x3 {
    float m[3][3];
};

struct Matrix4x4 {
    float m[4][4];
    // 行列同士の掛け算（新しい行列を返す）
    Matrix4x4 operator*(const Matrix4x4& rhs) const
    {
        Matrix4x4 result = {};

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[i][k] * rhs.m[k][j];
                }
                result.m[i][j] = sum;
            }
        }

        return result;
    }
};

class MathUtility {
public:
    // 正規化
    Vector3 Normalize(const Vector3& v);

    // 1.平行移動行列
    Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

    // 2.拡大縮小行列
    Matrix4x4 MakeScaleMatrix(const Vector3& scale);

    // 3.座標変換
    Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

    // 3.行列の積
    Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

    // 4.逆行列
    Matrix4x4 Inverse(const Matrix4x4& m);

    // 単位行列の作成
    Matrix4x4 MakeIdentity4x4();

    // 1.X軸回転行列
    Matrix4x4 MakeRotateXMatrix(float radian);

    // 2.Y軸回転行列
    Matrix4x4 MakeRotateYMatrix(float radian);

    // 3.Z軸回転行列
    Matrix4x4 MakeRotateZMatrix(float radian);

    // 3次元アフィン変換行列
    Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

    // 透視投影行列（透視変換行列）
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

    // 正射影行列
    Matrix4x4 MakeOrthograhicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

    // ビューポート変換行列
    Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
};
