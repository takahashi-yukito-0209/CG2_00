#include "MathTypes.h"

namespace Math {

// Vector3::operator- の定義
Vector3 Vector3::operator-(const Vector3 rhs) const
{
    return { x - rhs.x, y - rhs.y, z - rhs.z };
}

// Vector3::operator-() の定義
Vector3 Vector3::operator-() const
{
    return { -x, -y, -z };
}

// Matrix4x4::operator* の定義
Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const
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

} // namespace Math