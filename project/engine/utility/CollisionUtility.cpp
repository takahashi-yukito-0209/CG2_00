#include "CollisionUtility.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

using namespace Math;

namespace CollisionUtility {

// ヘルパー関数

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

// ベクトルを正規化して返す（長さがほぼゼロならそのまま返す）
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
    // OBB 中心から球中心へのベクトルを計算
    Vector3 d = Sub(s.center, obb.center);

    // OBB のローカル軸に沿って、球の中心に最も近い点を求める
    Vector3 closest = obb.center;
    // 各軸に対して、球の中心からの距離を計算し、OBB の半長さでクランプする
    for (int i = 0; i < 3; ++i) {
        // d を OBB の軸に投影して距離を求める
        float dist = Dot(d, obb.axis[i]);
        // 距離を半長さでクランプ
        if (dist > obb.halfLength[i]) {
            dist = obb.halfLength[i];
        }
        // 負の方向も同様にクランプ
        if (dist < -obb.halfLength[i]) {
            dist = -obb.halfLength[i];
        }

        // クランプされた距離を OBB の中心からのベクトルに変換して、最近接点を更新
        closest = Add(closest, Mul(obb.axis[i], dist));
    }

    // 最近接点と球の中心のベクトルを計算
    Vector3 diff = Sub(s.center, closest);
    // 距離の二乗が半径の二乗以下なら交差している
    return LengthSq(diff) <= s.radius * s.radius;
}

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

    // すべての軸で交差しているので、tmin と tmax の範囲内にヒットポイントが存在する
    if (outT) {
        // tmin が負の場合は、レイの開始点が AABB 内にあるので、tmax をヒット距離として使用する
        // もし tmax が負であればボックスはレイの後方にありヒットしない
        if (tmax < 0.0f) {
            return false;
        }

        // tmin が負であっても、tmax が正であればレイは AABB 内にあるので、tmax をヒット距離として使用する
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

} // namespace CollisionUtility

/// <summary>
/// Transform から AABB を作成するヘルパー
/// </summary>
namespace CollisionUtility {
AABB MakeAABBFromTransform(const Transform& t, const Vector3& halfLengths)
{
    // Transform から OBB を作成
    OBB obb = MakeOBBFromTransform(t, halfLengths);

    // OBB の8つのコーナーを計算
    Vector3 corners[8];
    // OBB の中心から各軸方向に半長さを加減してコーナーを求める
    int idx = 0;
    // sx, sy, sz はそれぞれ -1 と 1 を取ることで、8つのコーナーを生成
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                // OBB の中心から、各軸方向に半長さを加減してコーナーを求める
                Vector3 corner = obb.center;
                // 各軸方向に半長さを加減してコーナーを求める
                corner = Add(corner, Mul(obb.axis[0], obb.halfLength[0] * (float)sx)); // X軸方向に半長さを加減
                corner = Add(corner, Mul(obb.axis[1], obb.halfLength[1] * (float)sy)); // Y軸方向に半長さを加減
                corner = Add(corner, Mul(obb.axis[2], obb.halfLength[2] * (float)sz)); // Z軸方向に半長さを加減
                corners[idx++] = corner; // コーナーを配列に保存
            }
        }
    }

    // 8つのコーナーから AABB を作成
    Vector3 min = corners[0]; // 最初のコーナーを初期値として最小点を設定
    Vector3 max = corners[0]; // 最初のコーナーを初期値として最大点を設定
    // 8つのコーナーをループして、最小点と最大点を更新
    for (int i = 1; i < 8; ++i) {
        min.x = std::min(min.x, corners[i].x); // X軸方向の最小点を更新
        min.y = std::min(min.y, corners[i].y); // Y軸方向の最小点を更新
        min.z = std::min(min.z, corners[i].z); // Z軸方向の最小点を更新
        max.x = std::max(max.x, corners[i].x); // X軸方向の最大点を更新
        max.y = std::max(max.y, corners[i].y); // Y軸方向の最大点を更新
        max.z = std::max(max.z, corners[i].z); // Z軸方向の最大点を更新
    }

    // 最小点と最大点から AABB を作成して返す
    AABB box;
    box.min = min; // AABB の最小点を設定
    box.max = max; // AABB の最大点を設定
    return box; // AABB を返す
}
} // namespace CollisionUtility

/// <summary>
/// レイと三角形の交差判定（Möller–Trumbore アルゴリズム）
/// </summary>
namespace CollisionUtility {
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

} // namespace CollisionUtility

// ----------------------
// BVH 実装（簡易、トップダウンの median split）
// ----------------------
namespace CollisionUtility {

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

} // namespace CollisionUtility

// 詳細な当たり判定の実装
namespace CollisionUtility {

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

} // namespace CollisionUtility

/// <summary>
/// Transform から OBB を作成する関数
/// </summary>
namespace CollisionUtility {
OBB MakeOBBFromTransform(const Transform& t, const Vector3& halfLengths)
{
    // OBB を初期化する
    OBB obb {};

    // OBB の中心は、Transform の translate になる
    obb.center = t.translate;

    // Transform の回転をオイラー角から回転行列に変換する
    float cx = std::cos(t.rotate.x), sx = std::sin(t.rotate.x); // X軸回転のコサインとサインを計算する
    float cy = std::cos(t.rotate.y), sy = std::sin(t.rotate.y); // Y軸回転のコサインとサインを計算する
    float cz = std::cos(t.rotate.z), sz = std::sin(t.rotate.z); // Z軸回転のコサインとサインを計算する

    // 回転行列は、Z軸回転、Y軸回転、X軸回転の順で掛け合わせる（右手系の回転順序）
    // 回転行列の要素を計算する
    // 回転行列の要素は、以下のように計算される
    // Rz * Ry * Rx の順で回転行列を掛け合わせると、以下のような要素になる

    // 回転行列の (0,0) 要素を計算する
    float r00 = cy * cz;
    // 回転行列の (0,1) 要素を計算する
    float r01 = cy * sz;
    // 回転行列の (0,2) 要素を計算する
    float r02 = -sy;

    // 回転行列の (1,0) 要素を計算する
    float r10 = sx * sy * cz - cx * sz;
    // 回転行列の (1,1) 要素を計算する
    float r11 = sx * sy * sz + cx * cz;
    // 回転行列の (1,2) 要素を計算する
    float r12 = sx * cy;

    // 回転行列の (2,0) 要素を計算する
    float r20 = cx * sy * cz + sx * sz;
    // 回転行列の (2,1) 要素を計算する
    float r21 = cx * sy * sz - sx * cz;
    // 回転行列の (2,2) 要素を計算する
    float r22 = cx * cy;

    // 回転行列の要素を OBB の軸に設定する
    obb.axis[0] = NormalizeVec({ r00, r10, r20 }); // OBB の軸[0] に回転行列の第1列を設定（正規化）
    obb.axis[1] = NormalizeVec({ r01, r11, r21 }); // OBB の軸[1] に回転行列の第2列を設定（正規化）
    obb.axis[2] = NormalizeVec({ r02, r12, r22 }); // OBB の軸[2] に回転行列の第3列を設定（正規化）

    // OBB の半長さは、Transform の scale を掛けた halfLengths になる
    obb.halfLength[0] = halfLengths.x * t.scale.x; // OBB の半長さ[0] に halfLengths.x に Transform の scale.x を掛けた値を設定する
    obb.halfLength[1] = halfLengths.y * t.scale.y; // OBB の半長さ[1] に halfLengths.y に Transform の scale.y を掛けた値を設定する
    obb.halfLength[2] = halfLengths.z * t.scale.z; // OBB の半長さ[2] に halfLengths.z に Transform の scale.z を掛けた値を設定する

    // OBB を返す
    return obb;
}
} // namespace CollisionUtility
