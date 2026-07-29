#include "MathTypes.h"

namespace Math {

/// <summary>
/// Vector3 同士を加算する。
/// </summary>
Vector3 Vector3::operator+(const Vector3& rhs) const
{
    return { x + rhs.x, y + rhs.y, z + rhs.z };
}

/// <summary>
/// Vector3 同士を減算する。
/// </summary>
Vector3 Vector3::operator-(const Vector3& rhs) const
{
    return { x - rhs.x, y - rhs.y, z - rhs.z };
}

/// <summary>
/// Vector3 にスカラー値を掛ける。
/// </summary>
Vector3 Vector3::operator*(float scalar) const
{
    return { x * scalar, y * scalar, z * scalar };
}

/// <summary>
/// Vector3 をスカラー値で割る。
/// </summary>
Vector3 Vector3::operator/(float scalar) const
{
    return { x / scalar, y / scalar, z / scalar };
}

/// <summary>
/// Vector3 同士を加算して代入する。
/// </summary>
Vector3& Vector3::operator+=(const Vector3& rhs)
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}

/// <summary>
/// Vector3 同士を減算して代入する。
/// </summary>
Vector3& Vector3::operator-=(const Vector3& rhs)
{
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
}

/// <summary>
/// Vector3 にスカラー値を掛けて代入する。
/// </summary>
Vector3& Vector3::operator*=(float scalar)
{
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

/// <summary>
/// Vector3 をスカラー値で割って代入する。
/// </summary>
Vector3& Vector3::operator/=(float scalar)
{
    x /= scalar;
    y /= scalar;
    z /= scalar;
    return *this;
}

/// <summary>
/// Vector3 の符号を反転する。
/// </summary>
Vector3 Vector3::operator-() const
{
    return { -x, -y, -z };
}

/// <summary>
/// Matrix4x4 同士を乗算する。
/// </summary>
Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const
{
    Matrix4x4 result = {}; // 乗算結果

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f; // 行と列の内積
            for (int k = 0; k < 4; ++k) {
                sum += m[i][k] * rhs.m[k][j];
            }
            result.m[i][j] = sum;
        }
    }

    return result;
}

} // namespace Math