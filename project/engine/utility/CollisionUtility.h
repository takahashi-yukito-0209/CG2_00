#pragma once

#include "MathTypes.h"

#include <cstdint>
#include <vector>

/// <summary>
/// 衝突形状、交差判定、レイキャスト用のユーティリティ。
/// </summary>
namespace CollisionUtility {

// ----------------------
// 基本形状
// ----------------------

/// <summary>
/// 軸平行境界ボックスを表す構造体。
/// </summary>
struct AABB {
    Math::Vector3 min {}; // ボックスの最小点
    Math::Vector3 max {}; // ボックスの最大点
};

/// <summary>
/// 任意回転を許容する境界ボックスを表す構造体。
/// </summary>
struct OBB {
    Math::Vector3 center {}; // ボックス中心
    Math::Vector3 axis[3] {}; // ローカル軸
    float halfLength[3] {}; // 各軸方向の半長さ
};

/// <summary>
/// 球を表す構造体。
/// </summary>
struct Sphere {
    Math::Vector3 center {}; // 球の中心
    float radius = 0.0f; // 球の半径
};

/// <summary>
/// カプセルを表す構造体。
/// </summary>
struct Capsule {
    Math::Vector3 start {}; // カプセル軸の開始点
    Math::Vector3 end {}; // カプセル軸の終了点
    float radius = 0.0f; // カプセルの半径
};

/// <summary>
/// レイを表す構造体。
/// </summary>
struct Ray {
    Math::Vector3 origin {}; // レイの開始点
    Math::Vector3 dir {}; // レイ方向
};

/// <summary>
/// 三角形を表す構造体。
/// </summary>
struct Triangle {
    Math::Vector3 a {}; // 頂点A
    Math::Vector3 b {}; // 頂点B
    Math::Vector3 c {}; // 頂点C
};

/// <summary>
/// 三角形リストで構成されるメッシュを表す構造体。
/// </summary>
struct Mesh {
    std::vector<Triangle> triangles; // 三角形リスト
};

// ----------------------
// 返却データ
// ----------------------

/// <summary>
/// 衝突詳細情報を返すための構造体。
/// </summary>
struct CollisionResult {
    bool hit = false; // 衝突したか
    Math::Vector3 point {}; // 接触点
    Math::Vector3 normal {}; // 衝突法線
    float penetration = 0.0f; // 貫入深度
};

/// <summary>
/// レイのヒット情報を返すための構造体。
/// </summary>
struct RayHitResult {
    bool hit = false; // ヒットしたか
    float t = 0.0f; // レイ開始点からの距離
    Math::Vector3 point {}; // ヒット位置
    Math::Vector3 normal {}; // ヒット法線
};

/// <summary>
/// 接触点を表す構造体。
/// </summary>
struct ContactPoint {
    Math::Vector3 position {}; // 接触位置
    Math::Vector3 normal {}; // 接触法線
    float penetration = 0.0f; // 貫入深度
};

/// <summary>
/// 複数の接触点をまとめる構造体。
/// </summary>
struct ContactManifold {
    std::vector<ContactPoint> contacts; // 接触点リスト
};

// ----------------------
// 形状補助
// ----------------------

/// <summary>
/// AABB の中心座標を取得する。
/// </summary>
Math::Vector3 GetAABBCenter(const AABB& box);

/// <summary>
/// AABB の各軸方向の半分サイズを取得する。
/// </summary>
Math::Vector3 GetAABBHalfSize(const AABB& box);

/// <summary>
/// AABB の各軸方向のサイズを取得する。
/// </summary>
Math::Vector3 GetAABBSize(const AABB& box);

/// <summary>
/// 指定点を AABB 内に収めた最近点を取得する。
/// </summary>
Math::Vector3 ClosestPointAABB(const AABB& box, const Math::Vector3& point);

/// <summary>
/// 指定点を OBB 内に収めた最近点を取得する。
/// </summary>
Math::Vector3 ClosestPointOBB(const OBB& obb, const Math::Vector3& point);

/// <summary>
/// 指定点を球内に収めた最近点を取得する。
/// </summary>
Math::Vector3 ClosestPointSphere(const Sphere& sphere, const Math::Vector3& point);

/// <summary>
/// 指定点に最も近い線分上の点を取得する。
/// </summary>
Math::Vector3 ClosestPointSegment(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector3& point);

/// <summary>
/// 指定点が AABB 内に含まれているか判定する。
/// </summary>
bool ContainsPointAABB(const AABB& box, const Math::Vector3& point);

/// <summary>
/// 指定点が球内に含まれているか判定する。
/// </summary>
bool ContainsPointSphere(const Sphere& sphere, const Math::Vector3& point);

/// <summary>
/// 2つの AABB を内包する AABB を作成する。
/// </summary>
AABB MergeAABB(const AABB& a, const AABB& b);

/// <summary>
/// AABB を指定量だけ外側へ広げる。
/// </summary>
AABB ExpandAABB(const AABB& box, const Math::Vector3& padding);

/// <summary>
/// 球を内包する AABB を作成する。
/// </summary>
AABB GetSphereAABB(const Sphere& sphere);

/// <summary>
/// カプセルを内包する AABB を作成する。
/// </summary>
AABB GetCapsuleAABB(const Capsule& capsule);

/// <summary>
/// OBB を内包する AABB を作成する。
/// </summary>
AABB GetOBBAABB(const OBB& obb);

/// <summary>
/// Transform から OBB を作成する。
/// </summary>
OBB MakeOBBFromTransform(const Math::Transform& transform, const Math::Vector3& halfLengths);

/// <summary>
/// Transform から AABB を作成する。
/// </summary>
AABB MakeAABBFromTransform(const Math::Transform& transform, const Math::Vector3& halfLengths);

// ----------------------
// 基本交差判定
// ----------------------

/// <summary>
/// AABB 同士の交差を判定する。
/// </summary>
bool IntersectAABB_AABB(const AABB& a, const AABB& b);

/// <summary>
/// 球同士の交差を判定する。
/// </summary>
bool IntersectSphere_Sphere(const Sphere& a, const Sphere& b);

/// <summary>
/// AABB と球の交差を判定する。
/// </summary>
bool IntersectAABB_Sphere(const AABB& box, const Sphere& sphere);

/// <summary>
/// OBB 同士の交差を判定する。
/// </summary>
bool IntersectOBB_OBB(const OBB& a, const OBB& b);

/// <summary>
/// 球と OBB の交差を判定する。
/// </summary>
bool IntersectSphere_OBB(const Sphere& sphere, const OBB& obb);

/// <summary>
/// カプセル同士の交差を判定する。
/// </summary>
bool IntersectCapsule_Capsule(const Capsule& a, const Capsule& b);

/// <summary>
/// カプセルと球の交差を判定する。
/// </summary>
bool IntersectCapsule_Sphere(const Capsule& capsule, const Sphere& sphere);

/// <summary>
/// カプセルと AABB の交差を判定する。
/// </summary>
bool IntersectCapsule_AABB(const Capsule& capsule, const AABB& box);

/// <summary>
/// カプセルと OBB の交差を判定する。
/// </summary>
bool IntersectCapsule_OBB(const Capsule& capsule, const OBB& obb);

// ----------------------
// Collider
// ----------------------

using LayerMask = uint32_t;

/// <summary>
/// レイヤーマスクを作成する。
/// </summary>
inline LayerMask MakeLayerMask(int layer) { return (layer >= 0 && layer < 32) ? (1u << layer) : 0u; }

/// <summary>
/// 2つのレイヤーとマスクから衝突対象か判定する。
/// </summary>
inline bool ShouldCollide(LayerMask layerA, LayerMask collideMaskA, LayerMask layerB, LayerMask collideMaskB)
{
    return ((collideMaskA & layerB) != 0u) && ((collideMaskB & layerA) != 0u);
}

/// <summary>
/// Collider が保持する形状種別。
/// </summary>
enum class ColliderType {
    AABB,
    OBB,
    Sphere,
    Capsule,
    Mesh
};

/// <summary>
/// 衝突判定に使うCollider情報。
/// </summary>
struct Collider {
    ColliderType type = ColliderType::AABB; // 使用する形状種別
    LayerMask layer = MakeLayerMask(0); // 所属レイヤー
    LayerMask collideMask = 0xFFFFFFFFu; // 衝突対象レイヤーマスク
    AABB aabb; // AABB形状
    OBB obb; // OBB形状
    Sphere sphere; // 球形状
    Capsule capsule; // カプセル形状
    Mesh mesh; // メッシュ形状
};

/// <summary>
/// 2つのColliderが衝突しているか判定する。
/// </summary>
bool IntersectCollider(const Collider& a, const Collider& b);

/// <summary>
/// Collider を内包する AABB を取得する。
/// </summary>
AABB GetColliderAABB(const Collider& collider);

/// <summary>
/// レイと Collider の最近接交差を取得する。
/// </summary>
RayHitResult RayIntersectCollider(const Ray& ray, const Collider& collider, bool useMeshBvh = true);

// ----------------------
// レイキャスト
// ----------------------

/// <summary>
/// レイと AABB の交差を判定する。
/// </summary>
bool RayIntersectAABB(const Ray& ray, const AABB& box, float* outT = nullptr);

/// <summary>
/// レイと OBB の交差を判定する。
/// </summary>
bool RayIntersectOBB(const Ray& ray, const OBB& obb, float* outT = nullptr);

/// <summary>
/// レイと球の交差を判定する。
/// </summary>
bool RayIntersectSphere(const Ray& ray, const Sphere& sphere, float* outT = nullptr);

/// <summary>
/// レイとカプセルの交差を判定する。
/// </summary>
bool RayIntersectCapsule(const Ray& ray, const Capsule& capsule, float* outT = nullptr);

/// <summary>
/// レイと三角形の交差を判定する。
/// </summary>
bool RayIntersectTriangle(const Ray& ray, const Triangle& triangle, float* outT = nullptr, float* outU = nullptr, float* outV = nullptr);

/// <summary>
/// レイとメッシュの最近接交差を取得する。
/// </summary>
RayHitResult RayIntersectMesh(const Ray& ray, const Mesh& mesh);

// ----------------------
// 詳細判定
// ----------------------

/// <summary>
/// AABB 同士の詳細な交差情報を取得する。
/// </summary>
CollisionResult IntersectAABB_AABB_Detailed(const AABB& a, const AABB& b);

/// <summary>
/// 球同士の詳細な交差情報を取得する。
/// </summary>
CollisionResult IntersectSphere_Sphere_Detailed(const Sphere& a, const Sphere& b);

/// <summary>
/// AABB と球の詳細な交差情報を取得する。
/// </summary>
CollisionResult IntersectAABB_Sphere_Detailed(const AABB& box, const Sphere& sphere);

/// <summary>
/// 球と OBB の詳細な交差情報を取得する。
/// </summary>
CollisionResult IntersectSphere_OBB_Detailed(const Sphere& sphere, const OBB& obb);

/// <summary>
/// OBB 同士の詳細な交差情報を取得する。
/// </summary>
CollisionResult IntersectOBB_OBB_Detailed(const OBB& a, const OBB& b);

/// <summary>
/// レイと AABB の詳細な交差情報を取得する。
/// </summary>
RayHitResult RayIntersectAABB_Detailed(const Ray& ray, const AABB& box);

/// <summary>
/// レイと OBB の詳細な交差情報を取得する。
/// </summary>
RayHitResult RayIntersectOBB_Detailed(const Ray& ray, const OBB& obb);

/// <summary>
/// レイと球の詳細な交差情報を取得する。
/// </summary>
RayHitResult RayIntersectSphere_Detailed(const Ray& ray, const Sphere& sphere);

/// <summary>
/// レイとカプセルの詳細な交差情報を取得する。
/// </summary>
RayHitResult RayIntersectCapsule_Detailed(const Ray& ray, const Capsule& capsule);

/// <summary>
/// レイと三角形の詳細な交差情報を取得する。
/// </summary>
RayHitResult RayIntersectTriangle_Detailed(const Ray& ray, const Triangle& triangle);

// ----------------------
// BVH
// ----------------------

/// <summary>
/// BVH の1ノード分の情報。
/// </summary>
struct BVHNode {
    AABB bounds; // ノードが内包する境界
    int left = -1; // 左子ノード番号
    int right = -1; // 右子ノード番号
    int start = 0; // 三角形開始番号
    int count = 0; // 三角形数
};

/// <summary>
/// メッシュレイキャスト用の簡易BVH。
/// </summary>
struct BVH {
    std::vector<BVHNode> nodes; // BVHノード配列
    std::vector<Triangle> triangles; // BVH用に並べ替えた三角形配列

    /// <summary>
    /// 空のBVHを作成する。
    /// </summary>
    BVH() { }

    /// <summary>
    /// メッシュからBVHを構築する。
    /// </summary>
    explicit BVH(const Mesh& mesh);

    /// <summary>
    /// BVHを使ってレイキャストする。
    /// </summary>
    RayHitResult RayIntersect(const Ray& ray) const;
};

/// <summary>
/// BVHを使ってレイとメッシュの最近接交差を取得する。
/// </summary>
RayHitResult RayIntersectMesh_BVH(const Ray& ray, const Mesh& mesh);

// ----------------------
// 接触マニフォールド
// ----------------------

/// <summary>
/// AABB 同士の接触マニフォールドを作成する。
/// </summary>
ContactManifold CreateManifoldAABB_AABB(const AABB& a, const AABB& b);

/// <summary>
/// 球同士の接触マニフォールドを作成する。
/// </summary>
ContactManifold CreateManifoldSphere_Sphere(const Sphere& a, const Sphere& b);

/// <summary>
/// 球と OBB の接触マニフォールドを作成する。
/// </summary>
ContactManifold CreateManifoldSphere_OBB(const Sphere& sphere, const OBB& obb);

/// <summary>
/// OBB 同士の接触マニフォールドを作成する。
/// </summary>
ContactManifold CreateManifoldOBB_OBB(const OBB& a, const OBB& b);

} // namespace CollisionUtility