#include "CollisionUtility.h"
#include "mathUtility.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

using namespace Math;

namespace CollisionUtility {

// ----------------------
// 内部ヘルパー
// ----------------------

// ベクトルのドット積
static inline float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// ベクトルの減算
static inline Vector3 Sub(const Vector3& a, const Vector3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

// ベクトルの加算
static inline Vector3 Add(const Vector3& a, const Vector3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

// ベクトルのスカラー倍
static inline Vector3 Mul(const Vector3& v, float s)
{
    return { v.x * s, v.y * s, v.z * s };
}

// ベクトルの長さの二乗（距離比較に便利）
static inline float LengthSq(const Vector3& v)
{
    return Dot(v, v);
}

// ベクトルの外積
static inline Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
// ベクトルを正規化して返す（長さがほぼゼロならそのまま返す）
static inline float Clamp01(float value)
{
    return std::max(0.0f, std::min(value, 1.0f));
}

static inline Vector3 NormalizeVec(const Vector3& v)
{
    // 長さの二乗を計算
    float l2 = LengthSq(v);

    // 長さがほぼゼロの場合は正規化できないので、そのまま返す
    if (l2 <= 1e-12f) {
        return v;
    }

    // 長さの逆数を計算して、ベクトルに掛ける
    float inv = 1.0f / std::sqrt(l2);

    // 正規化されたベクトルを返す
    return { v.x * inv, v.y * inv, v.z * inv };
}


// 内部線分距離関数の前方宣言
float DistanceSqSegmentSegment(const Vector3& aStart, const Vector3& aEnd, const Vector3& bStart, const Vector3& bEnd);

/// <summary>
/// 指定点に最も近い三角形上の点を取得する。
/// </summary>
Vector3 ClosestPointTriangle(const Vector3& point, const Triangle& triangle)
{
    const Vector3 edgeAB = Sub(triangle.b, triangle.a); // AB辺ベクトル
    const Vector3 edgeAC = Sub(triangle.c, triangle.a); // AC辺ベクトル
    const Vector3 pointA = Sub(point, triangle.a); // Aから点へのベクトル
    const float d1 = Dot(edgeAB, pointA); // AB方向への投影
    const float d2 = Dot(edgeAC, pointA); // AC方向への投影
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return triangle.a;
    }

    const Vector3 pointB = Sub(point, triangle.b); // Bから点へのベクトル
    const float d3 = Dot(edgeAB, pointB); // AB方向への投影
    const float d4 = Dot(edgeAC, pointB); // AC方向への投影
    if (d3 >= 0.0f && d4 <= d3) {
        return triangle.b;
    }

    const float vertexRegionC = d1 * d4 - d3 * d2; // AB辺領域の判定値
    if (vertexRegionC <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float edgeRate = d1 / (d1 - d3); // AB辺上の最近割合
        return Add(triangle.a, Mul(edgeAB, edgeRate));
    }

    const Vector3 pointC = Sub(point, triangle.c); // Cから点へのベクトル
    const float d5 = Dot(edgeAB, pointC); // AB方向への投影
    const float d6 = Dot(edgeAC, pointC); // AC方向への投影
    if (d6 >= 0.0f && d5 <= d6) {
        return triangle.c;
    }

    const float vertexRegionB = d5 * d2 - d1 * d6; // AC辺領域の判定値
    if (vertexRegionB <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float edgeRate = d2 / (d2 - d6); // AC辺上の最近割合
        return Add(triangle.a, Mul(edgeAC, edgeRate));
    }

    const float vertexRegionA = d3 * d6 - d5 * d4; // BC辺領域の判定値
    if (vertexRegionA <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float edgeRate = (d4 - d3) / ((d4 - d3) + (d5 - d6)); // BC辺上の最近割合
        return Add(triangle.b, Mul(Sub(triangle.c, triangle.b), edgeRate));
    }

    const float denominator = 1.0f / (vertexRegionA + vertexRegionB + vertexRegionC); // 面内座標の分母
    const float v = vertexRegionB * denominator; // B方向の面内割合
    const float w = vertexRegionC * denominator; // C方向の面内割合
    return Add(triangle.a, Add(Mul(edgeAB, v), Mul(edgeAC, w)));
}

/// <summary>
/// 点と三角形の距離の二乗を取得する。
/// </summary>
float DistanceSqPointTriangle(const Vector3& point, const Triangle& triangle)
{
    const Vector3 closestPoint = ClosestPointTriangle(point, triangle); // 三角形上の最近点
    return LengthSq(Sub(point, closestPoint));
}

/// <summary>
/// 線分と三角形の距離の二乗を取得する。
/// </summary>
float DistanceSqSegmentTriangle(const Vector3& start, const Vector3& end, const Triangle& triangle)
{
    const Vector3 segment = Sub(end, start); // 線分方向
    const float segmentLength = std::sqrt(LengthSq(segment)); // 線分長
    if (segmentLength <= 1e-8f) {
        return DistanceSqPointTriangle(start, triangle);
    }

    const Ray segmentRay { start, segment }; // 線分をレイとして扱う
    float hitDistance = 0.0f; // 線分と三角形の交差距離
    if (RayIntersectTriangle(segmentRay, triangle, &hitDistance, nullptr, nullptr) && hitDistance >= 0.0f && hitDistance <= segmentLength) {
        return 0.0f;
    }

    float minDistanceSquared = (std::min)(DistanceSqPointTriangle(start, triangle), DistanceSqPointTriangle(end, triangle)); // 端点と三角形の最小距離
    minDistanceSquared = (std::min)(minDistanceSquared, DistanceSqSegmentSegment(start, end, triangle.a, triangle.b));
    minDistanceSquared = (std::min)(minDistanceSquared, DistanceSqSegmentSegment(start, end, triangle.b, triangle.c));
    minDistanceSquared = (std::min)(minDistanceSquared, DistanceSqSegmentSegment(start, end, triangle.c, triangle.a));
    return minDistanceSquared;
}

/// <summary>
/// 2つの線分間の距離の二乗を取得する。
/// </summary>
float DistanceSqSegmentSegment(const Vector3& aStart, const Vector3& aEnd, const Vector3& bStart, const Vector3& bEnd)
{
    constexpr float kSegmentEpsilon = 1e-8f; // 線分を点として扱う最小長さ
    const Vector3 segmentA = Sub(aEnd, aStart); // 1本目の線分方向
    const Vector3 segmentB = Sub(bEnd, bStart); // 2本目の線分方向
    const Vector3 startDifference = Sub(aStart, bStart); // 始点同士の差分
    const float lengthA = Dot(segmentA, segmentA); // 1本目の線分長の二乗
    const float lengthB = Dot(segmentB, segmentB); // 2本目の線分長の二乗
    const float projectionB = Dot(segmentB, startDifference); // 2本目方向への始点差分投影
    float parameterA = 0.0f; // 1本目の最近点割合
    float parameterB = 0.0f; // 2本目の最近点割合

    if (lengthA <= kSegmentEpsilon && lengthB <= kSegmentEpsilon) {
        return LengthSq(startDifference);
    }

    if (lengthA <= kSegmentEpsilon) {
        parameterB = Clamp01(projectionB / lengthB);
    } else {
        const float projectionA = Dot(segmentA, startDifference); // 1本目方向への始点差分投影
        if (lengthB <= kSegmentEpsilon) {
            parameterA = Clamp01(-projectionA / lengthA);
        } else {
            const float crossProjection = Dot(segmentA, segmentB); // 線分方向同士の投影
            const float denominator = lengthA * lengthB - crossProjection * crossProjection; // 連立計算の分母
            if (denominator != 0.0f) {
                parameterA = Clamp01((crossProjection * projectionB - projectionA * lengthB) / denominator);
            }
            parameterB = (crossProjection * parameterA + projectionB) / lengthB;
            if (parameterB < 0.0f) {
                parameterB = 0.0f;
                parameterA = Clamp01(-projectionA / lengthA);
            } else if (parameterB > 1.0f) {
                parameterB = 1.0f;
                parameterA = Clamp01((crossProjection - projectionA) / lengthA);
            }
        }
    }

    const Vector3 closestA = Add(aStart, Mul(segmentA, parameterA)); // 1本目の最近点
    const Vector3 closestB = Add(bStart, Mul(segmentB, parameterB)); // 2本目の最近点
    return LengthSq(Sub(closestA, closestB));
}

/// <summary>
/// 線分と半径分だけ拡張したAABBの交差を判定する。
/// </summary>
bool IntersectSegmentExpandedAABB(const Vector3& start, const Vector3& end, const AABB& box, float radius)
{
    const AABB expandedBox = ExpandAABB(box, { radius, radius, radius }); // カプセル半径で拡張したAABB
    if (ContainsPointAABB(expandedBox, start) || ContainsPointAABB(expandedBox, end)) {
        return true;
    }

    const Ray segmentRay { start, Sub(end, start) }; // 線分を始点と方向で表したレイ
    float hitDistance = 0.0f; // レイと拡張AABBの交差距離
    if (!RayIntersectAABB(segmentRay, expandedBox, &hitDistance)) {
        return false;
    }

    const float segmentLength = std::sqrt(LengthSq(Sub(end, start))); // 線分長
    return hitDistance >= 0.0f && hitDistance <= segmentLength;
}
/// <summary>
/// AABB の中心座標を取得する。
/// </summary>
Vector3 GetAABBCenter(const AABB& box)
{
    return Mul(Add(box.min, box.max), 0.5f);
}

/// <summary>
/// AABB の各軸方向の半分サイズを取得する。
/// </summary>
Vector3 GetAABBHalfSize(const AABB& box)
{
    return Mul(Sub(box.max, box.min), 0.5f);
}

/// <summary>
/// AABB の各軸方向のサイズを取得する。
/// </summary>
Vector3 GetAABBSize(const AABB& box)
{
    return Sub(box.max, box.min);
}

/// <summary>
/// 指定点を AABB 内に収めた最近点を取得する。
/// </summary>
Vector3 ClosestPointAABB(const AABB& box, const Vector3& point)
{
    return {
        std::max(box.min.x, std::min(point.x, box.max.x)),
        std::max(box.min.y, std::min(point.y, box.max.y)),
        std::max(box.min.z, std::min(point.z, box.max.z))
    };
}

/// <summary>
/// 指定点が AABB 内に含まれているか判定する。
/// </summary>
bool ContainsPointAABB(const AABB& box, const Vector3& point)
{
    return point.x >= box.min.x && point.x <= box.max.x
        && point.y >= box.min.y && point.y <= box.max.y
        && point.z >= box.min.z && point.z <= box.max.z;
}

/// <summary>
/// 2 つの AABB を内包する AABB を作成する。
/// </summary>
AABB MergeAABB(const AABB& a, const AABB& b)
{
    AABB merged {}; // 結合後の AABB
    merged.min = {
        std::min(a.min.x, b.min.x),
        std::min(a.min.y, b.min.y),
        std::min(a.min.z, b.min.z)
    };
    merged.max = {
        std::max(a.max.x, b.max.x),
        std::max(a.max.y, b.max.y),
        std::max(a.max.z, b.max.z)
    };
    return merged;
}

/// <summary>
/// AABB を指定量だけ外側へ広げる。
/// </summary>
AABB ExpandAABB(const AABB& box, const Vector3& padding)
{
    AABB expanded {}; // 拡張後の AABB
    expanded.min = Sub(box.min, padding);
    expanded.max = Add(box.max, padding);
    return expanded;
}
/// <summary>
/// 指定点を OBB 内に収めた最近点を取得する。
/// </summary>
Vector3 ClosestPointOBB(const OBB& obb, const Vector3& point)
{
    Vector3 distanceFromCenter = Sub(point, obb.center); // OBB 中心から指定点へのベクトル
    Vector3 closest = obb.center; // OBB 内の最近点

    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        float projectedLength = Dot(distanceFromCenter, obb.axis[axisIndex]); // OBB 軸上への投影距離
        float clampedLength = std::max(-obb.halfLength[axisIndex], std::min(projectedLength, obb.halfLength[axisIndex])); // OBB 内に収めた投影距離
        closest = Add(closest, Mul(obb.axis[axisIndex], clampedLength));
    }

    return closest;
}

/// <summary>
/// 指定点が球内に含まれているか判定する。
/// </summary>
bool ContainsPointSphere(const Sphere& sphere, const Vector3& point)
{
    Vector3 difference = Sub(point, sphere.center); // 球中心から指定点へのベクトル
    return LengthSq(difference) <= sphere.radius * sphere.radius;
}

/// <summary>
/// 指定点を球内に収めた最近点を取得する。
/// </summary>
Vector3 ClosestPointSphere(const Sphere& sphere, const Vector3& point)
{
    Vector3 difference = Sub(point, sphere.center); // 球中心から指定点へのベクトル
    float distanceSquared = LengthSq(difference); // 球中心から指定点までの距離の二乗

    if (distanceSquared <= sphere.radius * sphere.radius || distanceSquared <= 1e-12f) {
        return point;
    }

    float distance = std::sqrt(distanceSquared); // 球中心から指定点までの距離
    float scale = sphere.radius / distance; // 球面上まで縮める倍率
    return Add(sphere.center, Mul(difference, scale));
}
/// <summary>
/// 指定点に最も近い線分上の点を取得する。
/// </summary>
Vector3 ClosestPointSegment(const Vector3& start, const Vector3& end, const Vector3& point)
{
    const Vector3 segment = Sub(end, start); // 線分方向
    const float segmentLengthSquared = LengthSq(segment); // 線分長の二乗
    if (segmentLengthSquared <= 1e-12f) {
        return start;
    }

    const float rate = Clamp01(Dot(Sub(point, start), segment) / segmentLengthSquared); // 線分上の最近割合
    return Add(start, Mul(segment, rate));
}

/// <summary>
/// カプセルを内包する AABB を作成する。
/// </summary>
AABB GetCapsuleAABB(const Capsule& capsule)
{
    AABB bounds {}; // カプセルを内包するAABB
    bounds.min = {
        std::min(capsule.start.x, capsule.end.x) - capsule.radius,
        std::min(capsule.start.y, capsule.end.y) - capsule.radius,
        std::min(capsule.start.z, capsule.end.z) - capsule.radius
    };
    bounds.max = {
        std::max(capsule.start.x, capsule.end.x) + capsule.radius,
        std::max(capsule.start.y, capsule.end.y) + capsule.radius,
        std::max(capsule.start.z, capsule.end.z) + capsule.radius
    };
    return bounds;
}

/// <summary>
/// 球を内包する AABB を作成する。
/// </summary>
AABB GetSphereAABB(const Sphere& sphere)
{
    Vector3 radiusVector { sphere.radius, sphere.radius, sphere.radius }; // 各軸方向の半径
    AABB bounds {}; // 球を内包する AABB
    bounds.min = Sub(sphere.center, radiusVector);
    bounds.max = Add(sphere.center, radiusVector);
    return bounds;
}

/// <summary>
/// OBB を内包する AABB を作成する。
/// </summary>
AABB GetOBBAABB(const OBB& obb)
{
    Vector3 corner = Add(Add(obb.center, Mul(obb.axis[0], -obb.halfLength[0])), Add(Mul(obb.axis[1], -obb.halfLength[1]), Mul(obb.axis[2], -obb.halfLength[2]))); // 最初の頂点
    AABB bounds { corner, corner }; // OBB を内包する AABB

    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                Vector3 current = obb.center; // 現在計算中の頂点
                current = Add(current, Mul(obb.axis[0], obb.halfLength[0] * static_cast<float>(sx)));
                current = Add(current, Mul(obb.axis[1], obb.halfLength[1] * static_cast<float>(sy)));
                current = Add(current, Mul(obb.axis[2], obb.halfLength[2] * static_cast<float>(sz)));

                bounds.min.x = std::min(bounds.min.x, current.x);
                bounds.min.y = std::min(bounds.min.y, current.y);
                bounds.min.z = std::min(bounds.min.z, current.z);
                bounds.max.x = std::max(bounds.max.x, current.x);
                bounds.max.y = std::max(bounds.max.y, current.y);
                bounds.max.z = std::max(bounds.max.z, current.z);
            }
        }
    }

    return bounds;
}

// ----------------------
// 基本交差判定
// ----------------------

/// <summary>
/// AABB と AABB の交差判定
/// </summary>
bool IntersectAABB_AABB(const AABB& a, const AABB& b)
{
    // X軸で分離しているかをチェック
    if (a.max.x < b.min.x || a.min.x > b.max.x) {
        return false;
    }
    // Y軸で分離しているかをチェック
    if (a.max.y < b.min.y || a.min.y > b.max.y) {
        return false;
    }
    // Z軸で分離しているかをチェック
    if (a.max.z < b.min.z || a.min.z > b.max.z) {
        return false;
    }

    return true; // すべての軸で重なっているので交差している
}

/// <summary>
/// 球と球の交差判定
/// </summary>
bool IntersectSphere_Sphere(const Sphere& a, const Sphere& b)
{
    // 中心間のベクトル
    Vector3 d = Sub(a.center, b.center);
    // 半径の和
    float r = a.radius + b.radius;
    // 距離の二乗が半径の和の二乗以下なら交差している
    return LengthSq(d) <= r * r;
}

/// <summary>
/// AABB と球の交差判定
/// </summary>
bool IntersectAABB_Sphere(const AABB& box, const Sphere& s)
{
    // 球の中心に最も近いAABB上の点を求める
    float cx = std::max(box.min.x, std::min(s.center.x, box.max.x)); // X軸方向の最近接点
    float cy = std::max(box.min.y, std::min(s.center.y, box.max.y)); // Y軸方向の最近接点
    float cz = std::max(box.min.z, std::min(s.center.z, box.max.z)); // Z軸方向の最近接点

    // 最近接点と球の中心の距離を計算
    Vector3 closest { cx, cy, cz };
    // 最近接点と球の中心のベクトル
    Vector3 d = Sub(s.center, closest);
    // 距離の二乗が半径の二乗以下なら交差している
    return LengthSq(d) <= s.radius * s.radius;
}

/// <summary>
/// OBB と OBB の交差判定
/// </summary>
bool IntersectOBB_OBB(const OBB& A, const OBB& B)
{
    // Gottschalk らのクラシックな OBB-OBB 判定に従う
    float EPSILON = 1e-6f; // 数値誤差を避けるための小さな値
    float R[3][3]; // 回転行列 R = A^T * B
    float AbsR[3][3]; // 絶対値の回転行列

    // A のローカル軸を B のローカル軸に投影する回転行列を計算
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = Dot(A.axis[i], B.axis[j]);
        }
    }

    // A の中心から B の中心へのベクトル
    Vector3 tVec = Sub(B.center, A.center);
    // t を A の座標系に変換
    float t[3] = { Dot(tVec, A.axis[0]), Dot(tVec, A.axis[1]), Dot(tVec, A.axis[2]) };

    // 数値誤差を避けるため、回転行列の絶対値を計算
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            AbsR[i][j] = std::fabs(R[i][j]) + EPSILON;
        }
    }

    float ra, rb; // 投影半長さを格納する変数

    // 軸 L = A0, A1, A2 をテスト
    for (int i = 0; i < 3; ++i) {
        // A の軸 i に対する投影半長さ
        ra = A.halfLength[i];
        // B の軸 i に対する投影半長さ
        rb = B.halfLength[0] * AbsR[i][0] + B.halfLength[1] * AbsR[i][1] + B.halfLength[2] * AbsR[i][2];
        // t[i] は A の軸 i に対する中心間の距離の投影
        if (std::fabs(t[i]) > ra + rb) {
            return false; // 分離しているので交差していない
        }
    }

    // 軸 L = B0, B1, B2 をテスト
    for (int j = 0; j < 3; ++j) {

        // A の軸 j に対する投影半長さ
        ra = A.halfLength[0] * AbsR[0][j] + A.halfLength[1] * AbsR[1][j] + A.halfLength[2] * AbsR[2][j];
        // B の軸 j に対する投影半長さ
        rb = B.halfLength[j];
        // t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j] は A の中心から B の中心へのベクトルを B の軸 j に投影した値
        float tj = std::fabs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]);

        // tj は B の軸 j に対する中心間の距離の投影
        if (tj > ra + rb) {
            return false; // 分離しているので交差していない
        }
    }

    // 軸 L = A0 x B0 をテスト
    // A の軸 1 と 2 に対する投影半長さ
    ra = A.halfLength[1] * AbsR[2][0] + A.halfLength[2] * AbsR[1][0];
    // B の軸 1 と 2 に対する投影半長さ
    rb = B.halfLength[1] * AbsR[0][2] + B.halfLength[2] * AbsR[0][1];
    // t[2] * R[1][0] - t[1] * R[2][0] は A の中心から B の中心へのベクトルを軸 L = A0 x B0 に投影した値
    if (std::fabs(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A0 x B1
    // A の軸 1 と 2 に対する投影半長さ
    ra = A.halfLength[1] * AbsR[2][1] + A.halfLength[2] * AbsR[1][1];
    // B の軸 0 と 2 に対する投影半長さ
    rb = B.halfLength[0] * AbsR[0][2] + B.halfLength[2] * AbsR[0][0];
    // t[2] * R[1][1] - t[1] * R[2][1] は A の中心から B の中心へのベクトルを軸 L = A0 x B1 に投影した値
    if (std::fabs(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A0 x B2
    // A の軸 1 と 2 に対する投影半長さ
    ra = A.halfLength[1] * AbsR[2][2] + A.halfLength[2] * AbsR[1][2];
    // B の軸 0 と 1 に対する投影半長さ
    rb = B.halfLength[0] * AbsR[0][1] + B.halfLength[1] * AbsR[0][0];
    // t[2] * R[1][2] - t[1] * R[2][2] は A の中心から B の中心へのベクトルを軸 L = A0 x B2 に投影した値
    if (std::fabs(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A1 x B0
    // A の軸 0 と 2 に対する投影半長さ
    ra = A.halfLength[0] * AbsR[2][0] + A.halfLength[2] * AbsR[0][0];
    // B の軸 1 と 2 に対する投影半長さ
    rb = B.halfLength[1] * AbsR[1][2] + B.halfLength[2] * AbsR[1][1];
    // t[0] * R[2][0] - t[2] * R[0][0] は A の中心から B の中心へのベクトルを軸 L = A1 x B0 に投影した値
    if (std::fabs(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A1 x B1
    // A の軸 0 と 2 に対する投影半長さ
    ra = A.halfLength[0] * AbsR[2][1] + A.halfLength[2] * AbsR[0][1];
    // B の軸 0 と 2 に対する投影半長さ
    rb = B.halfLength[0] * AbsR[1][2] + B.halfLength[2] * AbsR[1][0];
    // t[0] * R[2][1] - t[2] * R[0][1] は A の中心から B の中心へのベクトルを軸 L = A1 x B1 に投影した値
    if (std::fabs(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A1 x B2
    // A の軸 0 と 2 に対する投影半長さ
    ra = A.halfLength[0] * AbsR[2][2] + A.halfLength[2] * AbsR[0][2];
    // B の軸 0 と 1 に対する投影半長さ
    rb = B.halfLength[0] * AbsR[1][1] + B.halfLength[1] * AbsR[1][0];
    // t[0] * R[2][2] - t[2] * R[0][2] は A の中心から B の中心へのベクトルを軸 L = A1 x B2 に投影した値
    if (std::fabs(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A2 x B0
    // A の軸 0 と 1 に対する投影半長さ
    ra = A.halfLength[0] * AbsR[1][0] + A.halfLength[1] * AbsR[0][0];
    // B の軸 1 と 2 に対する投影半長さ
    rb = B.halfLength[1] * AbsR[2][2] + B.halfLength[2] * AbsR[2][1];
    // t[1] * R[0][0] - t[0] * R[1][0] は A の中心から B の中心へのベクトルを軸 L = A2 x B0 に投影した値
    if (std::fabs(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A2 x B1
    // A の軸 0 と 1 に対する投影半長さ
    ra = A.halfLength[0] * AbsR[1][1] + A.halfLength[1] * AbsR[0][1];
    // B の軸 0 と 2 に対する投影半長さ
    rb = B.halfLength[0] * AbsR[2][2] + B.halfLength[2] * AbsR[2][0];
    // t[1] * R[0][1] - t[0] * R[1][1] は A の中心から B の中心へのベクトルを軸 L = A2 x B1 に投影した値
    if (std::fabs(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // 軸 L = A2 x B2
    // A の軸 0 と 1 に対する投影半長さ
    ra = A.halfLength[0] * AbsR[1][2] + A.halfLength[1] * AbsR[0][2];
    // B の軸 0 と 1 に対する投影半長さ
    rb = B.halfLength[0] * AbsR[2][1] + B.halfLength[1] * AbsR[2][0];
    // t[1] * R[0][2] - t[0] * R[1][2] は A の中心から B の中心へのベクトルを軸 L = A2 x B2 に投影した値
    if (std::fabs(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) {
        return false; // 分離しているので交差していない
    }

    // すべての軸で分離していないので交差している
    return true;
}

/// <summary>
/// 球と OBB の交差判定
/// </summary>
bool IntersectSphere_OBB(const Sphere& s, const OBB& obb)
{
    Vector3 closest = ClosestPointOBB(obb, s.center); // 球中心に最も近い OBB 内の点
    Vector3 diff = Sub(s.center, closest); // 最近接点から球中心へのベクトル
    return LengthSq(diff) <= s.radius * s.radius;
}

/// <summary>
/// カプセル同士の交差を判定する。
/// </summary>
bool IntersectCapsule_Capsule(const Capsule& a, const Capsule& b)
{
    const float radius = a.radius + b.radius; // 半径の合計
    return DistanceSqSegmentSegment(a.start, a.end, b.start, b.end) <= radius * radius;
}

/// <summary>
/// カプセルと球の交差を判定する。
/// </summary>
bool IntersectCapsule_Sphere(const Capsule& capsule, const Sphere& sphere)
{
    const Vector3 closestPoint = ClosestPointSegment(capsule.start, capsule.end, sphere.center); // 球中心に最も近いカプセル軸点
    const float radius = capsule.radius + sphere.radius; // 半径の合計
    return LengthSq(Sub(sphere.center, closestPoint)) <= radius * radius;
}

/// <summary>
/// カプセルと AABB の交差を判定する。
/// </summary>
bool IntersectCapsule_AABB(const Capsule& capsule, const AABB& box)
{
    return IntersectSegmentExpandedAABB(capsule.start, capsule.end, box, capsule.radius);
}

/// <summary>
/// カプセルと OBB の交差を判定する。
/// </summary>
bool IntersectCapsule_OBB(const Capsule& capsule, const OBB& obb)
{
    const Vector3 localStart {
        Dot(Sub(capsule.start, obb.center), obb.axis[0]),
        Dot(Sub(capsule.start, obb.center), obb.axis[1]),
        Dot(Sub(capsule.start, obb.center), obb.axis[2])
    }; // OBBローカル空間のカプセル開始点
    const Vector3 localEnd {
        Dot(Sub(capsule.end, obb.center), obb.axis[0]),
        Dot(Sub(capsule.end, obb.center), obb.axis[1]),
        Dot(Sub(capsule.end, obb.center), obb.axis[2])
    }; // OBBローカル空間のカプセル終了点
    const AABB localBox {
        { -obb.halfLength[0], -obb.halfLength[1], -obb.halfLength[2] },
        { obb.halfLength[0], obb.halfLength[1], obb.halfLength[2] }
    }; // OBBをローカルAABBとして扱う範囲
    return IntersectSegmentExpandedAABB(localStart, localEnd, localBox, capsule.radius);
}
/// <summary>
/// カプセルとメッシュの交差を判定する。
/// </summary>
bool IntersectCapsule_Mesh(const Capsule& capsule, const Mesh& mesh)
{
    if (mesh.triangles.empty()) {
        return false;
    }

    Collider meshCollider {}; // メッシュAABB計算用の一時Collider
    meshCollider.type = ColliderType::Mesh;
    meshCollider.mesh = mesh;
    if (!IntersectAABB_AABB(GetCapsuleAABB(capsule), GetColliderAABB(meshCollider))) {
        return false;
    }

    const float radiusSquared = capsule.radius * capsule.radius; // カプセル半径の二乗
    for (const Triangle& triangle : mesh.triangles) {
        if (DistanceSqSegmentTriangle(capsule.start, capsule.end, triangle) <= radiusSquared) {
            return true;
        }
    }

    return false;
}
// ----------------------
// 基本レイキャスト
// ----------------------

/// <summary>
/// レイと AABB の交差判定（スラブ法）
/// </summary>
bool RayIntersectAABB(const Ray& ray, const AABB& box, float* outT)
{
    // 正規化された方向ベクトルを使うことで、出力される t が実距離になるようにする
    Vector3 ndir = NormalizeVec(ray.dir);

    // tmin と tmax を初期化
    float tmin = -std::numeric_limits<float>::infinity(); // レイの開始点からの距離の最小値
    float tmax = std::numeric_limits<float>::infinity(); // レイの開始点からの距離の最大値

    // 各軸に対してスラブ法を適用
    auto check = [&](float origin, float dir, float minB, float maxB) -> bool {
        // レイがスラブと平行な場合の処理
        if (std::fabs(dir) < 1e-8f) {
            // レイがスラブと平行：原点がスラブ内にないとヒットしない
            return origin >= minB && origin <= maxB;
        }

        // レイがスラブと交差する場合の t1 と t2 を計算
        float t1 = (minB - origin) / dir; // レイがスラブの最小面と交差する距離
        float t2 = (maxB - origin) / dir; // レイがスラブの最大面と交差する距離

        // t1 と t2 をソートして、tmin と tmax を更新
        if (t1 > t2) {
            std::swap(t1, t2);
        }

        // tmin と tmax を更新
        if (t1 > tmin) {
            tmin = t1;
        }

        // tmax を更新
        if (t2 < tmax) {
            tmax = t2;
        }

        // tmin が tmax より大きい場合は、レイが AABB を通過しない
        if (tmin > tmax) {
            return false;
        }

        // tmax が負の場合は、AABB がレイの後ろにあるのでヒットしない
        return true;
    };

    // X軸のスラブをチェック
    if (!check(ray.origin.x, ndir.x, box.min.x, box.max.x)) {
        return false;
    }

    // Y軸のスラブをチェック
    if (!check(ray.origin.y, ndir.y, box.min.y, box.max.y)) {
        return false;
    }

    // Z軸のスラブをチェック
    if (!check(ray.origin.z, ndir.z, box.min.z, box.max.z)) {
        return false;
    }

    // すべての軸で交差していても、交差範囲がレイの後方だけにある場合はヒットしない
    if (tmax < 0.0f) {
        return false;
    }

    if (outT) {
        // tmin が負の場合は、レイの開始点が AABB 内にあるので、tmax をヒット距離として使用する
        *outT = tmin >= 0.0f ? tmin : tmax; // ndir を使って計算しているので t は実距離
    }

    return true; // ヒットしている
}

/// <summary>
/// レイと OBB の交差判定
/// </summary>
bool RayIntersectOBB(const Ray& ray, const OBB& obb, float* outT)
{
    // 正規化されたレイ方向を使う
    Vector3 ndir = NormalizeVec(ray.dir);

    // レイの原点を OBB のローカル空間に変換
    Vector3 localOrigin = {
        Dot(Sub(ray.origin, obb.center), obb.axis[0]),
        Dot(Sub(ray.origin, obb.center), obb.axis[1]),
        Dot(Sub(ray.origin, obb.center), obb.axis[2])
    };
    // レイの方向も OBB のローカル軸に投影して変換
    Vector3 localDir = {
        Dot(ndir, obb.axis[0]),
        Dot(ndir, obb.axis[1]),
        Dot(ndir, obb.axis[2])
    };

    // OBB をローカル空間の AABB として扱い、レイとの交差判定を行う
    Ray localRay { localOrigin, localDir };
    // OBB の半長さを使ってローカル空間の AABB を定義
    AABB box {
        { -obb.halfLength[0], -obb.halfLength[1], -obb.halfLength[2] },
        { obb.halfLength[0], obb.halfLength[1], obb.halfLength[2] }
    };

    // ローカル空間でのレイと AABB の交差判定を行う
    return RayIntersectAABB(localRay, box, outT);
}

/// <summary>
/// レイと球の交差判定
/// </summary>
bool RayIntersectSphere(const Ray& ray, const Sphere& s, float* outT)
{
    // 正規化されたレイ方向を使う
    Vector3 ndir = NormalizeVec(ray.dir);

    // レイの原点から球の中心へのベクトル m を計算
    Vector3 m = Sub(ray.origin, s.center);
    // b はレイの方向と m のドット積で、レイが球の中心からどれだけ離れているかを表す値
    float b = Dot(m, ndir);
    // c はレイの原点が球の中心からどれだけ離れているかを表す値で、半径と比較する
    float c = Dot(m, m) - s.radius * s.radius;
    // c > 0 かつ b > 0 の場合、レイの原点は球の外側にあり、レイは球から離れているため、交差しない
    if (c > 0.0f && b > 0.0f) {
        return false;
    }

    // 判別式を計算して、レイが球と交差するかを判断
    float discr = b * b - c;
    // 判別式が負の場合、レイは球と交差しない
    if (discr < 0.0f) {
        return false;
    }

    // レイは球と交差するので、交差点までの距離 t を計算
    float t = -b - std::sqrt(discr);
    // t が負の場合、レイの原点は球の内部にあるので、t を 0 にクランプして交差点をレイの原点にする
    if (t < 0.0f) {
        t = 0.0f;
    }

    // 交差点までの距離 t を出力パラメータに設定
    if (outT) {
        *outT = t; // ndir を使って計算しているので t は実距離
    }

    // レイは球と交差している
    return true;
}

// ----------------------
// Transform から衝突形状を作成
// ----------------------

/// <summary>
/// Transform から AABB を作成する。
/// </summary>
AABB MakeAABBFromTransform(const Transform& t, const Vector3& halfLengths)
{
    OBB obb = MakeOBBFromTransform(t, halfLengths); // Transform から作成した OBB
    return GetOBBAABB(obb);
}

// ----------------------
// 三角形とメッシュのレイキャスト
// ----------------------

/// <summary>
/// レイと三角形の交差判定（Möller–Trumbore アルゴリズム）。
/// </summary>
bool RayIntersectTriangle(const Ray& ray, const Triangle& tri, float* outT, float* outU, float* outV)
{
    // Möller–Trumbore アルゴリズムの実装
    const float EPSILON = 1e-8f;
    // レイ方向を正規化して、出力 t が実距離になるようにする
    Vector3 ndir = NormalizeVec(ray.dir);
    // 三角形の辺を計算
    Vector3 edge1 = Sub(tri.b, tri.a); // 辺1は頂点aから頂点bへのベクトル
    Vector3 edge2 = Sub(tri.c, tri.a); // 辺2は頂点aから頂点cへのベクトル

    // レイの方向と辺2の外積を計算
    Vector3 h = {
        ndir.y * edge2.z - ndir.z * edge2.y,
        ndir.z * edge2.x - ndir.x * edge2.z,
        ndir.x * edge2.y - ndir.y * edge2.x
    };

    // 辺1とhのドット積を計算
    float a = Dot(edge1, h);

    // a が 0 に近い場合、レイは三角形と平行であるため、交差しない
    if (a > -EPSILON && a < EPSILON) {
        return false; // レイは三角形と平行
    }

    // 交差点までの距離を計算するための逆数を計算
    float f = 1.0f / a;

    // レイの原点から頂点aへのベクトルを計算
    Vector3 s = Sub(ray.origin, tri.a);

    // u パラメータを計算
    float u = f * Dot(s, h);

    // u が 0 より小さいか 1 より大きい場合、交差点は三角形の外にあるため、交差しない
    if (u < 0.0f || u > 1.0f) {
        return false; // 交差点は三角形の外
    }

    // レイの方向と辺1の外積を計算
    Vector3 q = {
        s.y * edge1.z - s.z * edge1.y,
        s.z * edge1.x - s.x * edge1.z,
        s.x * edge1.y - s.y * edge1.x
    };

    // v パラメータを計算
    float v = f * Dot(ndir, q);

    // v が 0 より小さいか u + v が 1 より大きい場合、交差点は三角形の外にあるため、交差しない
    if (v < 0.0f || u + v > 1.0f) {
        return false; // 交差点は三角形の外
    }

    // 交差点までの距離 t を計算
    float t = f * Dot(edge2, q);

    // t が EPSILON より大きい場合、レイは三角形と交差している
    if (t > EPSILON) {

        // 交差点の距離 t とバリセントリック座標 u, v を出力パラメータに設定
        if (outT) {
            *outT = t; // 交差点までの距離を出力
        }

        // u と v は三角形の面内の位置を表すバリセントリック座標で、交差点が三角形のどこにあるかを示す
        if (outU) {
            *outU = u; // u を出力
        }

        // v は u と合わせて、交差点が三角形のどこにあるかを示すバリセントリック座標で、u + v <= 1 の範囲内で交差点が三角形の内部にあることを保証する
        if (outV) {
            *outV = v; // v を出力
        }

        return true; // レイは三角形と交差している
    }

    return false; // t が EPSILON 以下の場合、交差点はレイの原点から非常に近いか、レイの後ろにあるため、交差しない
}

/// <summary>
/// レイと三角形の交差判定を行い、交差している場合は交差点の位置や法線などの詳細な情報も返す関数
/// </summary>
RayHitResult RayIntersectTriangle_Detailed(const Ray& ray, const Triangle& tri)
{
    RayHitResult res; // 交差していない場合の初期値
    float t, u, v; // 交差判定を行うための変数

    // Möller–Trumbore アルゴリズムを使用して、レイと三角形の交差判定を行う
    if (!RayIntersectTriangle(ray, tri, &t, &u, &v)) {
        return res; // 交差していない場合は、hit フラグが false のままの res を返す
    }

    // 交差している場合は、hit フラグを true に設定し、交差点の距離 t と位置を計算して res に格納する
    res.hit = true; // レイは三角形と交差しているので、hit フラグを true に設定
    res.t = t; // 交差点までの距離 t を res に格納
    // レイ方向を正規化してから実距離で交差点を計算する
    Vector3 ndir = NormalizeVec(ray.dir);
    res.point = Add(ray.origin, Mul(ndir, t)); // 交差点の位置を計算して res に格納

    // 法線は三角形の法線
    Vector3 edge1 = Sub(tri.b, tri.a); // 辺1は頂点aから頂点bへのベクトル
    Vector3 edge2 = Sub(tri.c, tri.a); // 辺2は頂点aから頂点cへのベクトル
    Vector3 n = {
        edge1.y * edge2.z - edge1.z * edge2.y,
        edge1.z * edge2.x - edge1.x * edge2.z,
        edge1.x * edge2.y - edge1.y * edge2.x
    };

    // 法線を正規化
    float ln2 = LengthSq(n);

    // ln2 が非常に小さい場合は、三角形が非常に小さいか、頂点がほぼ同一位置にある可能性があるため、法線をゼロベクトルのままにする
    if (ln2 > 1e-6f) {
        float ln = std::sqrt(ln2); // 法線の長さを計算
        res.normal = { n.x / ln, n.y / ln, n.z / ln }; // 法線を正規化して res に格納
    }

    // ln2 が非常に小さい場合は、法線をゼロベクトルのままにする（交差点の位置が不安定なため）
    return res;
}

/// <summary>
/// レイとメッシュの交差判定を行い、最も近い交差点の情報を返す関数
/// </summary>
RayHitResult RayIntersectMesh(const Ray& ray, const Mesh& mesh)
{
    // すべての三角形に対してレイとの交差判定を行い、最も近い交差点の情報を保持する変数 best を初期化する
    RayHitResult best;
    best.t = std::numeric_limits<float>::infinity();

    // メッシュのすべての三角形に対して、レイとの交差判定を行うループ
    for (const Triangle& tri : mesh.triangles) {
        // 交差判定を行い、交差している場合は交差点の情報を取得する
        RayHitResult r = RayIntersectTriangle_Detailed(ray, tri);
        // 交差していて、かつ交差点までの距離 t がこれまでの最小値より小さい場合は、best を更新する
        if (r.hit && r.t < best.t) {
            best = r;
        }
    }

    // すべての三角形に対して交差判定を行った後、best に最も近い交差点の情報が格納されている。
    // もし best.t が無限大のままであれば、レイはメッシュと交差していないことになるので、hit フラグを false に設定する。
    if (best.t == std::numeric_limits<float>::infinity()) {
        best.hit = false;
    }

    // 最も近い交差点の情報を返す
    return best;
}

// ----------------------
// BVH 実装（簡易、トップダウンの median split）
// ----------------------

/// <summary>
/// AABB を三角形で拡張するヘルパー
/// </summary>
static void ExpandAABBByTriangle(AABB& box, const Triangle& t)
{
    box.min.x = std::min(box.min.x, std::min(t.a.x, std::min(t.b.x, t.c.x))); // AABB の最小点を三角形の頂点と比較して更新
    box.min.y = std::min(box.min.y, std::min(t.a.y, std::min(t.b.y, t.c.y))); // AABB の最小点を三角形の頂点と比較して更新
    box.min.z = std::min(box.min.z, std::min(t.a.z, std::min(t.b.z, t.c.z))); // AABB の最小点を三角形の頂点と比較して更新
    box.max.x = std::max(box.max.x, std::max(t.a.x, std::max(t.b.x, t.c.x))); // AABB の最大点を三角形の頂点と比較して更新
    box.max.y = std::max(box.max.y, std::max(t.a.y, std::max(t.b.y, t.c.y))); // AABB の最大点を三角形の頂点と比較して更新
    box.max.z = std::max(box.max.z, std::max(t.a.z, std::max(t.b.z, t.c.z))); // AABB の最大点を三角形の頂点と比較して更新
}

/// <summary>
/// 三角形の頂点から AABB を作成するヘルパー
/// </summary>
static AABB TriangleBounds(const Triangle& t)
{
    // 三角形の頂点から AABB を作成するために、三角形の3つの頂点の座標を比較して、最小点と最大点を求める
    AABB b;
    // AABB の最小点を三角形の頂点と比較して求める
    b.min = {
        std::min(t.a.x, std::min(t.b.x, t.c.x)),
        std::min(t.a.y, std::min(t.b.y, t.c.y)),
        std::min(t.a.z, std::min(t.b.z, t.c.z))
    };
    // AABB の最大点を三角形の頂点と比較して求める
    b.max = {
        std::max(t.a.x, std::max(t.b.x, t.c.x)),
        std::max(t.a.y, std::max(t.b.y, t.c.y)),
        std::max(t.a.z, std::max(t.b.z, t.c.z))
    };

    // 三角形の頂点から AABB を作成して返す
    return b;
}

/// <summary>
/// BVH を構築するための再帰関数。
/// </summary>
static int BuildBVHNode(BVH& bvh, int start, int count, int depth)
{
    // 新しいノードを作成して、三角形の範囲とバウンディングボックスを設定する
    int nodeIndex = (int)bvh.nodes.size();
    // ノードを追加して、三角形の範囲を設定する
    bvh.nodes.push_back(BVHNode());

    // 追加したノードの参照を取得して、三角形の範囲を設定する
    BVHNode& node = bvh.nodes.back();
    // start はこのノードが担当する三角形の開始インデックス、count はこのノードが担当する三角形の数を表す
    node.start = start; // ノードが担当する三角形の開始インデックスを設定
    node.count = count; // ノードが担当する三角形の数を設定

    // ノードのバウンディングボックスを、担当する三角形の頂点から計算して設定する
    AABB bounds;
    // バウンディングボックスの初期値を、無限大と負の無限大で初期化して、三角形の頂点と比較して更新する

    // bounds.min を無限大で初期化して、三角形の頂点と比較して更新する
    bounds.min = {
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity()
    };
    // bounds.max を負の無限大で初期化して、三角形の頂点と比較して更新する
    bounds.max = {
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };

    // 担当する三角形の範囲をループして、バウンディングボックスを拡張する
    for (int i = 0; i < count; ++i) {
        ExpandAABBByTriangle(bounds, bvh.triangles[start + i]);
    }

    // ノードのバウンディングボックスを設定する
    node.bounds = bounds;

    // 葉ノードの条件: 三角形の数が少ないか、深さが深すぎる場合は、これ以上分割せずに葉ノードとする
    if (count <= 2 || depth > 32) {
        node.left = node.right = -1;
        return nodeIndex;
    }

    // 分割軸を選択するために、バウンディングボックスの拡張を計算する
    Vector3 ext = { bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z };

    // 最も拡張の大きい軸を分割軸として選択する
    int axis = 0;
    // X軸、Y軸、Z軸の拡張を比較して、最も大きい軸を選択する
    if (ext.y > ext.x) {
        axis = 1;
    }
    // Z軸の拡張が選択した軸より大きい場合は、Z軸を分割軸として選択する
    if ((axis == 0 ? ext.x : ext.y) < ext.z) {
        axis = 2;
    }

    // 分割軸に沿って三角形をソートして、中央値で分割する
    std::sort(bvh.triangles.begin() + start, bvh.triangles.begin() + start + count, [&](const Triangle& a, const Triangle& b) {
        float ca = 0.0f, cb = 0.0f;
        // 分割軸に沿った三角形の重心を計算して、ソートの基準とする
        if (axis == 0) {
            ca = (a.a.x + a.b.x + a.c.x) / 3.0f;
            cb = (b.a.x + b.b.x + b.c.x) / 3.0f;
        } else if (axis == 1) {
            ca = (a.a.y + a.b.y + a.c.y) / 3.0f;
            cb = (b.a.y + b.b.y + b.c.y) / 3.0f;
        } else {
            ca = (a.a.z + a.b.z + a.c.z) / 3.0f;
            cb = (b.a.z + b.b.z + b.c.z) / 3.0f;
        }
        // ca と cb を比較して、ソートの順序を決定する
        return ca < cb;
    });

    // 中央で分割して、左右の子ノードを再帰的に構築する
    int mid = start + count / 2;
    // 左の子ノードを構築して、そのインデックスを node.left に設定する
    node.left = BuildBVHNode(bvh, start, mid - start, depth + 1);
    // 右の子ノードを構築して、そのインデックスを node.right に設定する
    node.right = BuildBVHNode(bvh, mid, start + count - mid, depth + 1);
    // 内部ノードなので、三角形の範囲はクリアしておく（オプション）
    return nodeIndex;
}

/// <summary>
/// メッシュから BVH を構築するコンストラクタ
/// </summary>
BVH::BVH(const Mesh& mesh)
{
    // メッシュの三角形を BVH の三角形リストにコピーする
    triangles = mesh.triangles;
    // BVH ノードのリストをクリアして、再帰的に BVH を構築する
    nodes.clear();
    // 三角形が存在する場合にのみ BVH を構築する
    if (!triangles.empty()) {
        BuildBVHNode(*this, 0, (int)triangles.size(), 0);
    }
}

/// <summary>
/// レイと BVH の交差判定を行い、最も近い交差点の情報を返す関数
/// </summary>
RayHitResult BVH::RayIntersect(const Ray& ray) const
{
    RayHitResult best;
    // 最も近い交差点の距離を無限大で初期化する
    best.t = std::numeric_limits<float>::infinity();
    // ノードが存在しない場合は、レイはメッシュと交差しないので、hit フラグを false に設定して返す
    if (nodes.empty()) {
        best.hit = false;
        return best;
    }

    // スタックを使用して、BVH を深さ優先で探索する
    std::vector<int> stack;
    // ルートノードのインデックスをスタックにプッシュして探索を開始する
    stack.push_back(0);
    // スタックが空になるまでループして、ノードを探索する
    while (!stack.empty()) {
        // スタックからノードのインデックスをポップして、そのノードを取得する
        int ni = stack.back();
        // スタックからノードのインデックスをポップする
        stack.pop_back();
        // ノードのインデックス ni を使用して、ノードを取得する
        const BVHNode& node = nodes[ni];
        // 交差判定を行うための変数 t を宣言
        float t;
        // レイとノードのバウンディングボックスの交差判定を行う。交差しない場合は、このノードとその子ノードをスキップする
        if (!RayIntersectAABB(ray, node.bounds, &t)) {
            continue;
        }

        // ノードとレイが交差する場合は、ノードが葉かどうかをチェックする。
        // 葉の場合は、担当する三角形に対してレイとの交差判定を行う。内部ノードの場合は、子ノードをスタックにプッシュして探索を続ける
        if (node.left == -1 && node.right == -1) {
            // 葉ノードの場合は、担当する三角形に対してレイとの交差判定を行う
            for (int i = 0; i < node.count; ++i) {
                // node.start + i で担当する三角形のインデックスを計算して、その三角形を取得する
                const Triangle& tri = triangles[node.start + i];
                // 交差判定を行い、交差している場合は交差点の情報を取得する
                RayHitResult r = RayIntersectTriangle_Detailed(ray, tri);
                // 交差していて、かつ交差点までの距離 t がこれまでの最小値より小さい場合は、best を更新する
                if (r.hit && r.t < best.t) {
                    best = r;
                }
            }
        } else {
            // 内部ノードの場合は、子ノードをスタックにプッシュして探索を続ける
            if (node.left != -1) {
                stack.push_back(node.left);
            }
            // 右の子ノードが存在する場合は、スタックにプッシュする
            if (node.right != -1) {
                stack.push_back(node.right);
            }
        }
    }

    // すべてのノードを探索した後、best に最も近い交差点の情報が格納されている
    if (best.t == std::numeric_limits<float>::infinity()) {
        best.hit = false; // レイはメッシュと交差していないので、hit フラグを false に設定する
    }

    // 最も近い交差点の情報を返す
    return best;
}

/// <summary>
/// レイとメッシュの交差判定を行い、最も近い交差点の情報を返す関数（BVH を使用して高速化）
/// </summary>
RayHitResult RayIntersectMesh_BVH(const Ray& ray, const Mesh& mesh)
{
    // メッシュから BVH を構築して、レイと BVH の交差判定を行う
    // ここでは同一メッシュに対して BVH をキャッシュして再構築を避ける
    static std::unordered_map<const Mesh*, BVH> g_bvhCache;
    // メッシュのポインタをキーとして BVH をキャッシュから検索する
    const Mesh* key = &mesh;
    // キャッシュに BVH が存在するかを検索する
    auto it = g_bvhCache.find(key);

    // キャッシュに BVH が存在しない場合は、構築してキャッシュに保存する
    if (it == g_bvhCache.end()) {
        // キャッシュに無ければ構築して保存
        BVH bvh(mesh);
        auto inserted = g_bvhCache.emplace(key, std::move(bvh));
        it = inserted.first;
    }

    // キャッシュから BVH を取得して、レイと BVH の交差判定を行う
    return it->second.RayIntersect(ray);
}

/// <summary>
/// 詳細な当たり判定の結果を ContactManifold に変換するヘルパー関数(AABB-AABB)
/// </summary>
ContactManifold CreateManifoldAABB_AABB(const AABB& a, const AABB& b)
{
    // ContactManifold を初期化する
    ContactManifold m;

    // AABB-AABB の詳細な当たり判定を行い、接触点、法線、貫入深度を取得する
    CollisionResult r = IntersectAABB_AABB_Detailed(a, b);
    // 当たっていない場合は、空の ContactManifold を返す
    if (!r.hit) {
        return m;
    }

    // 当たっている場合は、ContactPoint を作成して ContactManifold に追加する
    ContactPoint cp;
    // 接触点の位置を設定する
    cp.position = r.point;
    // 法線を設定する
    cp.normal = r.normal;
    // 貫入深度を設定する
    cp.penetration = r.penetration;
    // ContactManifold に ContactPoint を追加する
    m.contacts.push_back(cp);

    // ContactManifold を返す
    return m;
}

/// <summary>
/// 詳細な当たり判定の結果を ContactManifold に変換するヘルパー関数(Sphere-Sphere)
/// </summary>
ContactManifold CreateManifoldSphere_Sphere(const Sphere& a, const Sphere& b)
{
    // ContactManifold を初期化する
    ContactManifold m;

    // 球-球の詳細な当たり判定を行い、接触点、法線、貫入深度を取得する
    CollisionResult r = IntersectSphere_Sphere_Detailed(a, b);
    // 当たっていない場合は、空の ContactManifold を返す
    if (!r.hit) {
        return m;
    }

    // 当たっている場合は、ContactPoint を作成して ContactManifold に追加する
    ContactPoint cp;
    // 接触点の位置を設定する
    cp.position = r.point;
    // 法線を設定する
    cp.normal = r.normal;
    // 貫入深度を設定する
    cp.penetration = r.penetration;
    // ContactManifold に ContactPoint を追加する
    m.contacts.push_back(cp);

    // ContactManifold を返す
    return m;
}

/// <summary>
/// 詳細な当たり判定の結果を ContactManifold に変換するヘルパー関数(Sphere-OBB)
/// </summary>
ContactManifold CreateManifoldSphere_OBB(const Sphere& s, const OBB& obb)
{
    // ContactManifold を初期化する
    ContactManifold m;
    // 球-OBB の詳細な当たり判定を行い、接触点、法線、貫入深度を取得する
    CollisionResult r = IntersectSphere_OBB_Detailed(s, obb);
    // 当たっていない場合は、空の ContactManifold を返す
    if (!r.hit) {
        return m;
    }

    // 当たっている場合は、ContactPoint を作成して ContactManifold に追加する
    ContactPoint cp;
    // 接触点の位置を設定する
    cp.position = r.point;
    // 法線を設定する
    cp.normal = r.normal;
    // 貫入深度を設定する
    cp.penetration = r.penetration;
    // ContactManifold に ContactPoint を追加する
    m.contacts.push_back(cp);

    // ContactManifold を返す
    return m;
}

/// <summary>
/// 詳細な当たり判定の結果を ContactManifold に変換するヘルパー関数(OBB-OBB)
/// </summary>
ContactManifold CreateManifoldOBB_OBB(const OBB& A, const OBB& B)
{
    // ContactManifold を初期化する
    ContactManifold m;

    // OBB-OBB の詳細な当たり判定を行い、接触点、法線、貫入深度を取得する
    CollisionResult r = IntersectOBB_OBB_Detailed(A, B);
    // 当たっていない場合は、空の ContactManifold を返す
    if (!r.hit) {
        return m;
    }

    // 当たっている場合は、ContactPoint を作成して ContactManifold に追加する
    ContactPoint cp;
    // 接触点の位置を設定する
    cp.position = r.point;
    // 法線を設定する
    cp.normal = r.normal;
    // 貫入深度を設定する
    cp.penetration = r.penetration;
    // ContactManifold に ContactPoint を追加する
    m.contacts.push_back(cp);

    // ContactManifold を返す
    return m;
}

// ----------------------
// 詳細な当たり判定
// ----------------------

/// <summary>
/// AABB - AABB の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
CollisionResult IntersectAABB_AABB_Detailed(const AABB& a, const AABB& b)
{
    // CollisionResult を初期化する
    CollisionResult r;
    // AABB-AABB の交差判定を行う。交差していない場合は、hit フラグが false のままの r を返す
    if (!IntersectAABB_AABB(a, b)) {
        return r;
    }

    // 当たっている場合は、hit フラグを true に設定する
    r.hit = true;
    // 最小の貫入軸を見つける
    float penX = (std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x)); // X軸方向の貫入深度を計算する
    float penY = (std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y)); // Y軸方向の貫入深度を計算する
    float penZ = (std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z)); // Z軸方向の貫入深度を計算する

    // X軸、Y軸、Z軸の貫入深度を比較して、最小の貫入軸を見つける
    if (penX <= penY && penX <= penZ) {
        // 最小の貫入深度を r.penetration に設定する
        r.penetration = penX;
        // 法線は b の中心が a の中心より大きい場合は正、そうでない場合は負の方向を向くように設定する
        r.normal = { (b.min.x + b.max.x) / 2.0f > (a.min.x + a.max.x) / 2.0f ? 1.0f : -1.0f, 0.0f, 0.0f };

    } else if (penY <= penX && penY <= penZ) {
        // 最小の貫入深度を r.penetration に設定する
        r.penetration = penY;
        // 法線は b の中心が a の中心より大きい場合は正、そうでない場合は負の方向を向くように設定する
        r.normal = { 0.0f, (b.min.y + b.max.y) / 2.0f > (a.min.y + a.max.y) / 2.0f ? 1.0f : -1.0f, 0.0f };

    } else {
        // 最小の貫入深度を r.penetration に設定する
        r.penetration = penZ;
        // 法線は b の中心が a の中心より大きい場合は正、そうでない場合は負の方向を向くように設定する
        r.normal = { 0.0f, 0.0f, (b.min.z + b.max.z) / 2.0f > (a.min.z + a.max.z) / 2.0f ? 1.0f : -1.0f };
    }

    // 接触点は、重なり範囲の中心を計算して設定する
    r.point.x = (std::max(a.min.x, b.min.x) + std::min(a.max.x, b.max.x)) * 0.5f; // X軸方向の重なり範囲の中心を計算して r.point.x に設定する
    r.point.y = (std::max(a.min.y, b.min.y) + std::min(a.max.y, b.max.y)) * 0.5f; // Y軸方向の重なり範囲の中心を計算して r.point.y に設定する
    r.point.z = (std::max(a.min.z, b.min.z) + std::min(a.max.z, b.max.z)) * 0.5f; // Z軸方向の重なり範囲の中心を計算して r.point.z に設定する

    // CollisionResult を返す
    return r;
}

/// <summary>
/// 球 - 球の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
CollisionResult IntersectSphere_Sphere_Detailed(const Sphere& a, const Sphere& b)
{
    // CollisionResult を初期化する
    CollisionResult r;
    // 球-球の交差判定を行う。交差していない場合は、hit フラグが false のままの r を返す
    Vector3 d = Sub(b.center, a.center);

    // d の長さの二乗を計算する
    float dist2 = LengthSq(d);
    // 半径の和を計算する
    float rsum = a.radius + b.radius;
    // d の長さの二乗が半径の和の二乗より大きい場合は、球は交差していないので、r を返す
    if (dist2 > rsum * rsum) {
        return r;
    }

    // d の長さを計算する
    float dist = std::sqrt(dist2);
    // 当たっている場合は、hit フラグを true に設定する
    r.hit = true;
    // 貫入深度は、半径の和から中心間距離を引いた値になる
    r.penetration = rsum - dist;

    // 法線は、中心間ベクトルを正規化したものになる。
    // ただし、中心が重なっている特異ケースを考慮して、距離が非常に小さい場合は、法線を適当に設定する
    if (dist > 1e-6f) {
        // 法線を正規化して r.normal に設定する
        r.normal = { d.x / dist, d.y / dist, d.z / dist };
        // 接触点は、a の中心から法線方向に半径 - 貫入深度 * 0.5f の位置になる
        r.point = Add(a.center, Mul(r.normal, a.radius - r.penetration * 0.5f));
    } else {
        // 中心が重なっている特異ケースでは、法線を適当に設定する（例えば、X軸方向の単位ベクトル）
        r.normal = { 1.0f, 0.0f, 0.0f };
        // 接触点は、a の中心と b の中心の中間点になる
        r.point = a.center;
    }

    // CollisionResult を返す
    return r;
}

/// <summary>
/// AABB - 球の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
CollisionResult IntersectAABB_Sphere_Detailed(const AABB& box, const Sphere& s)
{
    // CollisionResult を初期化する
    CollisionResult r;
    // AABB-球の交差判定を行う。交差していない場合は、hit フラグが false のままの r を返す
    if (!IntersectAABB_Sphere(box, s)) {
        return r;
    }

    // 当たっている場合は、hit フラグを true に設定する
    r.hit = true;
    // 最近接点を計算する。球の中心を AABB の範囲内にクランプすることで、最近接点を求める
    float cx = std::max(box.min.x, std::min(s.center.x, box.max.x)); // X軸方向に球の中心を AABB の範囲内にクランプして cx に設定する
    float cy = std::max(box.min.y, std::min(s.center.y, box.max.y)); // Y軸方向に球の中心を AABB の範囲内にクランプして cy に設定する
    float cz = std::max(box.min.z, std::min(s.center.z, box.max.z)); // Z軸方向に球の中心を AABB の範囲内にクランプして cz に設定する
    // 最近接点を r.point に設定する
    r.point = { cx, cy, cz };

    // 法線は、球の中心と最近接点のベクトルを正規化したものになる。
    Vector3 diff = Sub(s.center, r.point);
    // diff の長さの二乗を計算する
    float dist2 = LengthSq(diff);
    // 距離が非常に小さい場合は、法線を適当に設定する（例えば、Y軸方向の単位ベクトル）して、貫入深度は球の半径とする
    float dist = std::sqrt(dist2);
    // 距離が十分に大きい場合は、法線を正規化して r.normal に設定し、貫入深度は球の半径から距離を引いた値になる
    if (dist > 1e-6f) {
        // 法線を正規化して r.normal に設定する
        r.normal = { diff.x / dist, diff.y / dist, diff.z / dist };
        // 貫入深度は、球の半径から距離を引いた値になる
        r.penetration = s.radius - dist;
    } else {
        // 距離が非常に小さい場合は、法線を適当に設定する（例えば、Y軸方向の単位ベクトル）して、貫入深度は球の半径とする
        float dx = std::min(s.center.x - box.min.x, box.max.x - s.center.x); // X軸方向の最近接距離を計算する
        float dy = std::min(s.center.y - box.min.y, box.max.y - s.center.y); // Y軸方向の最近接距離を計算する
        float dz = std::min(s.center.z - box.min.z, box.max.z - s.center.z); // Z軸方向の最近接距離を計算する

        // 最も距離が小さい軸を見つけて、その軸に沿った法線を設定し、貫入深度は球の半径とする
        if (dx <= dy && dx <= dz) {
            // 法線は、球の中心が AABB の中心より小さい場合は負、そうでない場合は正の方向を向くように設定する
            r.normal = { s.center.x - box.min.x < box.max.x - s.center.x ? -1.0f : 1.0f, 0.0f, 0.0f };
            // 貫入深度は、球の半径から最近接距離を引いた値になる
            r.penetration = (std::min(s.center.x - box.min.x, box.max.x - s.center.x));

        } else if (dy <= dx && dy <= dz) {
            // 法線は、球の中心が AABB の中心より小さい場合は負、そうでない場合は正の方向を向くように設定する
            r.normal = { 0.0f, s.center.y - box.min.y < box.max.y - s.center.y ? -1.0f : 1.0f, 0.0f };
            // 貫入深度は、球の半径から最近接距離を引いた値になる
            r.penetration = (std::min(s.center.y - box.min.y, box.max.y - s.center.y));

        } else {
            // 法線は、球の中心が AABB の中心より小さい場合は負、そうでない場合は正の方向を向くように設定する
            r.normal = { 0.0f, 0.0f, s.center.z - box.min.z < box.max.z - s.center.z ? -1.0f : 1.0f };
            // 貫入深度は、球の半径から最近接距離を引いた値になる
            r.penetration = (std::min(s.center.z - box.min.z, box.max.z - s.center.z));
        }
    }

    // CollisionResult を返す
    return r;
}

/// <summary>
/// 球 - OBB の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
CollisionResult IntersectSphere_OBB_Detailed(const Sphere& s, const OBB& obb)
{
    // CollisionResult を初期化する
    CollisionResult r;
    // 球-OBB の交差判定を行う。交差していない場合は、hit フラグが false のままの r を返す
    if (!IntersectSphere_OBB(s, obb)) {
        return r;
    }

    // 当たっている場合は、hit フラグを true に設定する
    r.hit = true;

    // 最近接点を計算する。球の中心から OBB の中心へのベクトルを OBB の軸に投影して、OBB の半長さでクランプすることで、最近接点を求める
    Vector3 d = Sub(s.center, obb.center);
    // 最近接点を obb.center からスタートして、OBB の軸に沿ってクランプした距離だけ移動させることで求める
    Vector3 closest = obb.center;
    // OBB の各軸について、球の中心から OBB の中心へのベクトルをその軸に投影して、OBB の半長さでクランプする
    for (int i = 0; i < 3; ++i) {
        // d を obb.axis[i] に投影する
        float dist = Dot(d, obb.axis[i]);
        // 投影した距離を OBB の半長さでクランプする
        float clamped = dist;

        // clamped を OBB の半長さでクランプする
        if (clamped > obb.halfLength[i]) {
            clamped = obb.halfLength[i];
        }

        // clamped を -OBB の半長さでクランプする
        if (clamped < -obb.halfLength[i]) {
            clamped = -obb.halfLength[i];
        }

        // 最近接点を obb.center からスタートして、obb.axis[i] に clamped だけ移動させる
        closest = Add(closest, Mul(obb.axis[i], clamped));
    }

    // 最近接点を r.point に設定する
    r.point = closest;
    // 法線は、球の中心と最近接点のベクトルを正規化したものになる。
    Vector3 diff = Sub(s.center, closest);
    // diff の長さの二乗を計算する
    float dist2 = LengthSq(diff);
    // 距離が非常に小さい場合は、法線を適当に設定する（例えば、obb.axis[0] の方向）して、貫入深度は球の半径とする
    float dist = std::sqrt(dist2);

    // 距離が十分に大きい場合は、法線を正規化して r.normal に設定し、貫入深度は球の半径から距離を引いた値になる
    if (dist > 1e-6f) {
        // 法線を正規化して r.normal に設定する
        r.normal = { diff.x / dist, diff.y / dist, diff.z / dist };
        // 貫入深度は、球の半径から距離を引いた値になる
        r.penetration = s.radius - dist;
    } else {
        // 距離が非常に小さい場合は、法線を適当に設定する（例えば、obb.axis[0] の方向）して、貫入深度は球の半径とする
        r.normal = obb.axis[0];
        // 貫入深度は、球の半径とする
        r.penetration = s.radius;
    }

    // CollisionResult を返す
    return r;
}

/// <summary>
/// OBB - OBB の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
CollisionResult IntersectOBB_OBB_Detailed(const OBB& A, const OBB& B)
{
    // CollisionResult を初期化する
    CollisionResult result;
    // OBB-OBB の交差判定を行う。交差していない場合は、hit フラグが false のままの result を返す
    if (!IntersectOBB_OBB(A, B)) {
        return result;
    }

    // 当たっている場合は、hit フラグを true に設定する
    result.hit = true;

    // ベクトル演算のヘルパーラムダ関数
    auto Cross = [](const Vector3& u, const Vector3& v) -> Vector3 {
        return {
            u.y * v.z - u.z * v.y,
            u.z * v.x - u.x * v.z,
            u.x * v.y - u.y * v.x
        };
    };

    // ベクトルの長さの二乗を計算するヘルパーラムダ関数
    auto Normalize = [](const Vector3& v) -> Vector3 {
        // ベクトルの長さの二乗を計算する
        float l2 = v.x * v.x + v.y * v.y + v.z * v.z;
        // 長さが非常に小さい場合は、正規化をスキップして元のベクトルを返す
        if (l2 <= 1e-12f) {
            return v;
        }
        // ベクトルを正規化するための逆長を計算する
        float inv = 1.0f / std::sqrt(l2);

        // ベクトルを正規化して返す
        return { v.x * inv, v.y * inv, v.z * inv };
    };

    // A の中心から B の中心へのベクトル
    Vector3 tVec = Sub(B.center, A.center);

    // 最小の貫入軸とその深さを見つけるための変数
    float minOverlap = std::numeric_limits<float>::infinity();
    // 最小の貫入軸を初期化する（適当な値で初期化しておく）
    Vector3 bestAxis = { 1.0f, 0.0f, 0.0f };

    // テストする軸のリスト：Aの軸、Bの軸、A_i x B_j
    std::vector<Vector3> axes;
    // 軸の数は最大で 15 個になるので、あらかじめリザーブしておく
    axes.reserve(15);

    // A の軸を追加する
    for (int i = 0; i < 3; ++i) {
        axes.push_back(A.axis[i]);
    }

    // B の軸を追加する
    for (int j = 0; j < 3; ++j) {
        axes.push_back(B.axis[j]);
    }

    // A_i x B_j の軸を追加する
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            // A_i x B_j を計算して軸のリストに追加する
            axes.push_back(Cross(A.axis[i], B.axis[j]));
        }
    }

    // 各軸について、分離軸テストを行う
    for (const Vector3& ax : axes) {
        // 軸の長さの二乗を計算する
        float axLen2 = LengthSq(ax);
        // 長さが非常に小さい軸は無視する（特に A_i x B_j の軸は、A と B の軸が平行な場合に長さがゼロになる可能性がある）
        if (axLen2 < 1e-12f) {
            continue;
        }

        // 軸を正規化する
        Vector3 axis = Normalize(ax);

        // A の投影半幅
        float ra = 0.0f;
        // A の各軸に対して、A の半長さを軸に投影した値を加算していく
        for (int k = 0; k < 3; ++k) {
            ra += A.halfLength[k] * std::fabs(Dot(A.axis[k], axis));
        }

        // B の投影半幅
        float rb = 0.0f;
        // B の各軸に対して、B の半長さを軸に投影した値を加算していく
        for (int k = 0; k < 3; ++k) {
            rb += B.halfLength[k] * std::fabs(Dot(B.axis[k], axis));
        }

        // A の中心から B の中心へのベクトルを軸に投影した値の絶対値を計算する
        float dist = std::fabs(Dot(tVec, axis));
        // 分離軸が見つかれば、重なりの深さは、A の投影半幅と B の投影半幅の和から、中心間距離を引いた値になる
        float overlap = (ra + rb) - dist;
        // 分離軸が見つかれば理論上ここで負になるはずだが、既に当たりは確認済みなので無視
        if (overlap < 0.0f) {
            continue;
        }

        // 最小の貫入軸と深さを更新する
        if (overlap < minOverlap) {
            minOverlap = overlap; // 最小の貫入深度を更新する
            bestAxis = axis; // 最小の貫入軸を更新する
        }
    }

    // 最小の貫入軸が見つからない場合は、特異ケースとして、A と B の中心を結ぶベクトルを法線とし、接触点は A と B の中心の中間点とする
    if (minOverlap == std::numeric_limits<float>::infinity()) {

        // A と B の中心を結ぶベクトルを計算する
        Vector3 between = Sub(B.center, A.center);
        // ベクトルの長さの二乗を計算する
        float d2 = LengthSq(between);
        // ベクトルの長さを計算する
        float d = std::sqrt(d2);

        // 距離が非常に小さい場合は、法線を適当に設定する（例えば、X軸方向の単位ベクトル）。そうでない場合は、法線を正規化して設定する
        if (d > 1e-6f) {
            // 法線を正規化して result.normal に設定する
            result.normal = { between.x / d, between.y / d, between.z / d };
        } else {
            // 距離が非常に小さい場合は、法線を適当に設定する（例えば、X軸方向の単位ベクトル）
            result.normal = { 1.0f, 0.0f, 0.0f };
        }

        // 貫入深度はゼロとする
        result.penetration = 0.0f;
        // 接触点は、A と B の中心の中間点とする
        result.point = Add(A.center, Mul(Sub(B.center, A.center), 0.5f));

        // CollisionResult を返す
        return result;
    }

    // 最小の貫入軸が見つかった場合は、その軸を法線とし、貫入深度を result.penetration に設定する
    if (Dot(bestAxis, tVec) < 0.0f) {
        // 法線の向きを、A から B へ向かうように反転する
        bestAxis = { -bestAxis.x, -bestAxis.y, -bestAxis.z };
    }

    // 法線を result.normal に設定する
    result.normal = bestAxis;
    // 貫入深度を result.penetration に設定する
    result.penetration = minOverlap;

    // 接触点は、A と B の重なりの中心になるように計算する

    // A の投影半幅
    float Aproj = 0.0f;
    // A の各軸に対して、A の半長さを軸に投影した値を加算していく
    for (int k = 0; k < 3; ++k) {
        Aproj += A.halfLength[k] * std::fabs(Dot(A.axis[k], bestAxis));
    }

    // B の投影半幅
    float Bproj = 0.0f;
    // B の各軸に対して、B の半長さを軸に投影した値を加算していく
    for (int k = 0; k < 3; ++k) {
        Bproj += B.halfLength[k] * std::fabs(Dot(B.axis[k], bestAxis));
    }

    // A の中心から B の中心へのベクトルを軸に投影した値を計算する
    Vector3 pointA = Add(A.center, Mul(bestAxis, Aproj));
    // B の中心から A の中心へのベクトルを軸に投影した値を計算する
    Vector3 pointB = Add(B.center, Mul(bestAxis, -Bproj));
    // 接触点は、pointA と pointB の中間点になるように設定する
    result.point = Mul(Add(pointA, pointB), 0.5f);

    // CollisionResult を返す
    return result;
}

/// <summary>
/// レイ - AABB の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
RayHitResult RayIntersectAABB_Detailed(const Ray& ray, const AABB& box)
{
    // RayHitResult を初期化する
    RayHitResult res;
    // 交差判定のための変数
    float t;

    // レイと AABB の交差判定を行う。交差していない場合は、hit フラグが false のままの res を返す
    if (!RayIntersectAABB(ray, box, &t)) {
        return res;
    }

    // 当たっている場合は、hit フラグを true に設定する
    res.hit = true;
    // 交点を計算する
    res.t = t;
    // 交点は、正規化したレイ方向を使って実距離で計算する
    Vector3 ndir = NormalizeVec(ray.dir);
    res.point = Add(ray.origin, Mul(ndir, t));

    // 法線は、交点が AABB のどの面に近いかを判断して、その面の法線を設定する
    Vector3 p = res.point;
    // 交点と AABB の各面の距離を計算する
    float dxMin = std::fabs(p.x - box.min.x); // X軸の最小面との距離を計算する
    float dxMax = std::fabs(box.max.x - p.x); // X軸の最大面との距離を計算する
    float dyMin = std::fabs(p.y - box.min.y); // Y軸の最小面との距離を計算する
    float dyMax = std::fabs(box.max.y - p.y); // Y軸の最大面との距離を計算する
    float dzMin = std::fabs(p.z - box.min.z); // Z軸の最小面との距離を計算する
    float dzMax = std::fabs(box.max.z - p.z); // Z軸の最大面との距離を計算する
    // 最小距離を dxMin で初期化する
    float minD = dxMin;
    // 最小距離に対応する法線を、X軸の最小面の法線で初期化する
    res.normal = { -1.0f, 0.0f, 0.0f };

    // 他の面との距離を比較して、最小距離と法線を更新する

    // X軸の最大面との距離を比較して、最小距離と法線を更新する
    if (dxMax < minD) {
        // 最小距離を dxMax に更新する
        minD = dxMax;
        // 法線を、X軸の最大面の法線に更新する
        res.normal = { 1.0f, 0.0f, 0.0f };
    }

    // Y軸の面との距離を比較して、最小距離と法線を更新する
    if (dyMin < minD) {
        // 最小距離を dyMin に更新する
        minD = dyMin;
        // 法線を、Y軸の最小面の法線に更新する
        res.normal = { 0.0f, -1.0f, 0.0f };
    }

    // Y軸の最大面との距離を比較して、最小距離と法線を更新する
    if (dyMax < minD) {
        // 最小距離を dyMax に更新する
        minD = dyMax;
        // 法線を、Y軸の最大面の法線に更新する
        res.normal = { 0.0f, 1.0f, 0.0f };
    }

    // Z軸の面との距離を比較して、最小距離と法線を更新する
    if (dzMin < minD) {
        // 最小距離を dzMin に更新する
        minD = dzMin;
        // 法線を、Z軸の最小面の法線に更新する
        res.normal = { 0.0f, 0.0f, -1.0f };
    }

    // Z軸の最大面との距離を比較して、最小距離と法線を更新する
    if (dzMax < minD) {
        // 最小距離を dzMax に更新する
        minD = dzMax;
        // 法線を、Z軸の最大面の法線に更新する
        res.normal = { 0.0f, 0.0f, 1.0f };
    }

    // RayHitResult を返す
    return res;
}

/// <summary>
/// レイ - OBB の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
RayHitResult RayIntersectOBB_Detailed(const Ray& ray, const OBB& obb)
{
    // RayHitResult を初期化する
    RayHitResult res;
    // 交差判定のための変数
    float t;

    // レイと OBB の交差判定を行う。交差していない場合は、hit フラグが false のままの res を返す
    if (!RayIntersectOBB(ray, obb, &t)) {
        return res;
    }

    // 当たっている場合は、hit フラグを true に設定する
    res.hit = true;
    // 交点を計算する
    res.t = t;
    // 交点は、正規化したレイ方向を使って実距離で計算する
    Vector3 ndir = NormalizeVec(ray.dir);
    res.point = Add(ray.origin, Mul(ndir, t));

    // 法線は、交点と OBB の中心を結ぶベクトルを正規化したものになる。
    Vector3 dir = Sub(res.point, obb.center);
    // dir の長さの二乗を計算する
    float len2 = LengthSq(dir);

    // 距離が非常に小さい場合は、法線を適当に設定する（例えば、obb.axis[0] の方向）。そうでない場合は、法線を正規化して設定する
    if (len2 > 1e-6f) {
        // 法線を正規化して res.normal に設定する
        float len = std::sqrt(len2);
        // 法線を正規化して res.normal に設定する
        res.normal = { dir.x / len, dir.y / len, dir.z / len };
    } else {
        // 距離が非常に小さい場合は、法線を適当に設定する（例えば、obb.axis[0] の方向）
        res.normal = obb.axis[0];
    }

    // RayHitResult を返す
    return res;
}

/// <summary>
/// レイ - 球の詳細な当たり判定を行い、接触点、法線、貫入深度を返す関数
/// </summary>
RayHitResult RayIntersectSphere_Detailed(const Ray& ray, const Sphere& s)
{
    // RayHitResult を初期化する
    RayHitResult res;
    // 交差判定のための変数
    float t;

    // レイと球の交差判定を行う。交差していない場合は、hit フラグが false のままの res を返す
    if (!RayIntersectSphere(ray, s, &t)) {
        return res;
    }

    // 当たっている場合は、hit フラグを true に設定する
    res.hit = true;
    // 交点を計算する
    res.t = t;
    // 交点は、正規化したレイ方向を使って実距離で計算する
    Vector3 ndir = NormalizeVec(ray.dir);
    res.point = Add(ray.origin, Mul(ndir, t));
    // 法線は、交点と球の中心を結ぶベクトルを正規化したものになる。
    Vector3 n = Sub(res.point, s.center);
    // n の長さの二乗を計算する
    float l2 = LengthSq(n);
    // 距離が非常に小さい場合は、法線を適当に設定する（例えば、X軸方向の単位ベクトル）。そうでない場合は、法線を正規化して設定する
    if (l2 > 1e-6f) {
        // 法線を正規化して res.normal に設定する
        float l = std::sqrt(l2);
        // 法線を正規化して res.normal に設定する
        res.normal = { n.x / l, n.y / l, n.z / l };
    } else {
        // 距離が非常に小さい場合は、法線を適当に設定する（例えば、X軸方向の単位ベクトル）
        res.normal = { 1.0f, 0.0f, 0.0f };
    }

    // RayHitResult を返す
    return res;
}

/// <summary>
/// レイとカプセルの詳細な交差情報を取得する。
/// </summary>
RayHitResult RayIntersectCapsule_Detailed(const Ray& ray, const Capsule& capsule)
{
    RayHitResult bestHit {}; // 最も近いヒット
    bestHit.t = std::numeric_limits<float>::infinity();
    const Vector3 rayDirection = NormalizeVec(ray.dir); // 正規化したレイ方向
    const Vector3 capsuleSegment = Sub(capsule.end, capsule.start); // カプセル軸線分
    const float capsuleLength = std::sqrt(LengthSq(capsuleSegment)); // カプセル軸の長さ
    const float radius = (std::max)(capsule.radius, 0.0f); // 判定に使う半径

    auto updateBestHit = [&](float candidateT, const Vector3& normalCenter) {
        if (candidateT < 0.0f || candidateT >= bestHit.t) {
            return;
        }
        bestHit.hit = true;
        bestHit.t = candidateT;
        bestHit.point = Add(ray.origin, Mul(rayDirection, candidateT));
        bestHit.normal = NormalizeVec(Sub(bestHit.point, normalCenter));
    }; // 最近ヒット更新処理

    if (capsuleLength <= 1e-8f) {
        Sphere sphere { capsule.start, radius }; // 軸が潰れた場合の代替球
        return RayIntersectSphere_Detailed(ray, sphere);
    }

    const Vector3 capsuleAxis = Mul(capsuleSegment, 1.0f / capsuleLength); // カプセル軸方向
    const Vector3 originFromStart = Sub(ray.origin, capsule.start); // 開始点からレイ原点へのベクトル
    const float originAxisDistance = Dot(originFromStart, capsuleAxis); // 軸方向の原点位置
    const float rayAxisDirection = Dot(rayDirection, capsuleAxis); // レイ方向の軸成分
    const Vector3 originRadial = Sub(originFromStart, Mul(capsuleAxis, originAxisDistance)); // 軸に垂直な原点成分
    const Vector3 rayRadial = Sub(rayDirection, Mul(capsuleAxis, rayAxisDirection)); // 軸に垂直なレイ成分
    const float a = Dot(rayRadial, rayRadial); // 円柱二次方程式のa
    const float b = 2.0f * Dot(originRadial, rayRadial); // 円柱二次方程式のb
    const float c = Dot(originRadial, originRadial) - radius * radius; // 円柱二次方程式のc

    if (a > 1e-8f) {
        const float discriminant = b * b - 4.0f * a * c; // 円柱交差の判別式
        if (discriminant >= 0.0f) {
            const float sqrtDiscriminant = std::sqrt(discriminant); // 判別式の平方根
            const float invDenominator = 0.5f / a; // 2aの逆数
            const float candidates[] = {
                (-b - sqrtDiscriminant) * invDenominator,
                (-b + sqrtDiscriminant) * invDenominator
            }; // 胴体側の交差候補
            for (float candidateT : candidates) {
                const float axisDistance = originAxisDistance + candidateT * rayAxisDirection; // 軸上の交差位置
                if (axisDistance >= 0.0f && axisDistance <= capsuleLength) {
                    const Vector3 normalCenter = Add(capsule.start, Mul(capsuleAxis, axisDistance)); // 法線基準になる軸上点
                    updateBestHit(candidateT, normalCenter);
                }
            }
        }
    }

    const Sphere startSphere { capsule.start, radius }; // 開始端球
    const Sphere endSphere { capsule.end, radius }; // 終了端球
    float sphereT = 0.0f; // 端球のヒット距離
    if (RayIntersectSphere(ray, startSphere, &sphereT)) {
        updateBestHit(sphereT, capsule.start);
    }
    if (RayIntersectSphere(ray, endSphere, &sphereT)) {
        updateBestHit(sphereT, capsule.end);
    }

    if (!bestHit.hit) {
        return {};
    }
    if (LengthSq(bestHit.normal) <= 1e-12f) {
        bestHit.normal = Mul(rayDirection, -1.0f);
    }
    return bestHit;
}

/// <summary>
/// レイとカプセルの交差判定。
/// </summary>
bool RayIntersectCapsule(const Ray& ray, const Capsule& capsule, float* outT)
{
    const RayHitResult hitResult = RayIntersectCapsule_Detailed(ray, capsule); // 詳細判定の結果
    if (!hitResult.hit) {
        return false;
    }
    if (outT) {
        *outT = hitResult.t;
    }
    return true;
}
// ----------------------
// Transform から衝突形状を作成
// ----------------------

/// <summary>
/// Transform から OBB を作成する。
/// </summary>
OBB MakeOBBFromTransform(const Transform& t, const Vector3& halfLengths)
{
    OBB obb {}; // 作成するOBB
    obb.center = t.translate;

    const Matrix4x4 rotateMatrix = MathUtil::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, t.rotate, { 0.0f, 0.0f, 0.0f }); // 描画Transformと同じ回転行列
    obb.axis[0] = NormalizeVec({ rotateMatrix.m[0][0], rotateMatrix.m[0][1], rotateMatrix.m[0][2] });
    obb.axis[1] = NormalizeVec({ rotateMatrix.m[1][0], rotateMatrix.m[1][1], rotateMatrix.m[1][2] });
    obb.axis[2] = NormalizeVec({ rotateMatrix.m[2][0], rotateMatrix.m[2][1], rotateMatrix.m[2][2] });

    obb.halfLength[0] = std::fabs(halfLengths.x * t.scale.x);
    obb.halfLength[1] = std::fabs(halfLengths.y * t.scale.y);
    obb.halfLength[2] = std::fabs(halfLengths.z * t.scale.z);
    return obb;
}

namespace {

/// <summary>
/// AABBを回転なしのOBBとして扱える形に変換する。
/// </summary>
OBB MakeOBBFromAABBShape(const AABB& aabb)
{
    OBB obb {}; // AABBを表すOBB
    const Vector3 halfSize = GetAABBHalfSize(aabb); // AABBの半分サイズ
    obb.center = GetAABBCenter(aabb);
    obb.axis[0] = { 1.0f, 0.0f, 0.0f };
    obb.axis[1] = { 0.0f, 1.0f, 0.0f };
    obb.axis[2] = { 0.0f, 0.0f, 1.0f };
    obb.halfLength[0] = halfSize.x;
    obb.halfLength[1] = halfSize.y;
    obb.halfLength[2] = halfSize.z;
    return obb;
}

} // namespace

/// <summary>
/// Collider を内包する AABB を取得する。
/// </summary>
AABB GetColliderAABB(const Collider& collider)
{
    if (collider.type == ColliderType::AABB) {
        return collider.aabb;
    }
    if (collider.type == ColliderType::OBB) {
        return GetOBBAABB(collider.obb);
    }
    if (collider.type == ColliderType::Sphere) {
        return GetSphereAABB(collider.sphere);
    }
    if (collider.type == ColliderType::Capsule) {
        return GetCapsuleAABB(collider.capsule);
    }

    AABB meshAabb {}; // メッシュ全体を内包するAABB
    if (collider.mesh.triangles.empty()) {
        return meshAabb;
    }

    const Triangle& firstTriangle = collider.mesh.triangles.front(); // 初期範囲に使う最初の三角形
    meshAabb.min = firstTriangle.a;
    meshAabb.max = firstTriangle.a;
    for (const Triangle& triangle : collider.mesh.triangles) {
        const Vector3 vertices[] = { triangle.a, triangle.b, triangle.c }; // AABBへ反映する三角形頂点
        for (const Vector3& vertex : vertices) {
            meshAabb.min.x = (std::min)(meshAabb.min.x, vertex.x);
            meshAabb.min.y = (std::min)(meshAabb.min.y, vertex.y);
            meshAabb.min.z = (std::min)(meshAabb.min.z, vertex.z);
            meshAabb.max.x = (std::max)(meshAabb.max.x, vertex.x);
            meshAabb.max.y = (std::max)(meshAabb.max.y, vertex.y);
            meshAabb.max.z = (std::max)(meshAabb.max.z, vertex.z);
        }
    }
    return meshAabb;
}

/// <summary>
/// レイと Collider の最近接交差を取得する。
/// </summary>
RayHitResult RayIntersectCollider(const Ray& ray, const Collider& collider, bool useMeshBvh)
{
    if (collider.type == ColliderType::AABB) {
        return RayIntersectAABB_Detailed(ray, collider.aabb);
    }
    if (collider.type == ColliderType::OBB) {
        return RayIntersectOBB_Detailed(ray, collider.obb);
    }
    if (collider.type == ColliderType::Sphere) {
        return RayIntersectSphere_Detailed(ray, collider.sphere);
    }
    if (collider.type == ColliderType::Capsule) {
        return RayIntersectCapsule_Detailed(ray, collider.capsule);
    }
    if (collider.type == ColliderType::Mesh) {
        return useMeshBvh ? RayIntersectMesh_BVH(ray, collider.mesh) : RayIntersectMesh(ray, collider.mesh);
    }

    return {};
}
/// <summary>
/// 2つのColliderが衝突しているか判定する。
/// </summary>
bool IntersectCollider(const Collider& a, const Collider& b)
{
    if (!ShouldCollide(a.layer, a.collideMask, b.layer, b.collideMask)) {
        return false;
    }

    if (a.type == ColliderType::AABB && b.type == ColliderType::AABB) {
        return IntersectAABB_AABB(a.aabb, b.aabb);
    }
    if (a.type == ColliderType::AABB && b.type == ColliderType::Sphere) {
        return IntersectAABB_Sphere(a.aabb, b.sphere);
    }
    if (a.type == ColliderType::Sphere && b.type == ColliderType::AABB) {
        return IntersectAABB_Sphere(b.aabb, a.sphere);
    }
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Sphere) {
        return IntersectSphere_Sphere(a.sphere, b.sphere);
    }
    if (a.type == ColliderType::OBB && b.type == ColliderType::OBB) {
        return IntersectOBB_OBB(a.obb, b.obb);
    }
    if (a.type == ColliderType::Sphere && b.type == ColliderType::OBB) {
        return IntersectSphere_OBB(a.sphere, b.obb);
    }
    if (a.type == ColliderType::OBB && b.type == ColliderType::Sphere) {
        return IntersectSphere_OBB(b.sphere, a.obb);
    }
    if (a.type == ColliderType::AABB && b.type == ColliderType::OBB) {
        return IntersectOBB_OBB(MakeOBBFromAABBShape(a.aabb), b.obb);
    }
    if (a.type == ColliderType::OBB && b.type == ColliderType::AABB) {
        return IntersectOBB_OBB(a.obb, MakeOBBFromAABBShape(b.aabb));
    }
    if (a.type == ColliderType::Capsule && b.type == ColliderType::Capsule) {
        return IntersectCapsule_Capsule(a.capsule, b.capsule);
    }
    if (a.type == ColliderType::Capsule && b.type == ColliderType::Sphere) {
        return IntersectCapsule_Sphere(a.capsule, b.sphere);
    }
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Capsule) {
        return IntersectCapsule_Sphere(b.capsule, a.sphere);
    }
    if (a.type == ColliderType::Capsule && b.type == ColliderType::AABB) {
        return IntersectCapsule_AABB(a.capsule, b.aabb);
    }
    if (a.type == ColliderType::AABB && b.type == ColliderType::Capsule) {
        return IntersectCapsule_AABB(b.capsule, a.aabb);
    }
    if (a.type == ColliderType::Capsule && b.type == ColliderType::OBB) {
        return IntersectCapsule_OBB(a.capsule, b.obb);
    }
    if (a.type == ColliderType::OBB && b.type == ColliderType::Capsule) {
        return IntersectCapsule_OBB(b.capsule, a.obb);
    }
    if (a.type == ColliderType::Capsule && b.type == ColliderType::Mesh) {
        return IntersectCapsule_Mesh(a.capsule, b.mesh);
    }
    if (a.type == ColliderType::Mesh && b.type == ColliderType::Capsule) {
        return IntersectCapsule_Mesh(b.capsule, a.mesh);
    }
    return false;
}
} // namespace CollisionUtility
