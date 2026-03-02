#pragma once

#include "MathTypes.h"
#include <cstdint>
#include <vector>

using namespace Math;

/// <summary>
/// 衝突判定ユーティリティ
/// </summary>
namespace CollisionUtility {

// 軸平行境界ボックス（AABB）
struct AABB {
    Vector3 min {}; // box最小点
    Vector3 max {}; // box最大点
};

// オブジェクトに対して回転を許容する境界ボックス（OBB）
struct OBB {
    Vector3 center {}; // box center
    Vector3 axis[3] {}; // ローカル軸（単位長さを想定）
    float halfLength[3] {}; // 各軸方向の半長さ
};

// 球
struct Sphere {
    Vector3 center {}; // 球の中心
    float radius = 0.0f; // 球の半径
};

// レイ
struct Ray {
    Vector3 origin {}; // レイの開始点
    Vector3 dir {}; // 正規化されていることが望ましい
};

// 三角形
struct Triangle {
    Vector3 a, b, c; // 頂点座標
};

// メッシュ（複数の三角形で構成される）
struct Mesh {
    std::vector<Triangle> triangles; // 三角形のリスト
};

// ----------------------
// 衝突判定関数
// ----------------------

/// <summary>
/// AABB と AABB の交差判定
/// </summary>
bool IntersectAABB_AABB(const AABB& a, const AABB& b);

/// <summary>
/// 球と球の交差判定
/// </summary>
bool IntersectSphere_Sphere(const Sphere& a, const Sphere& b);

/// <summary>
/// AABB と球の交差判定
/// </summary>
bool IntersectAABB_Sphere(const AABB& box, const Sphere& s);

/// <summary>
/// OBB と OBB の交差判定
/// </summary>
bool IntersectOBB_OBB(const OBB& A, const OBB& B);

/// <summary>
/// OBB と球の交差判定
/// </summary>
bool IntersectSphere_OBB(const Sphere& s, const OBB& obb);

// ----------------------
// レイと衝突判定関数
// ----------------------

/// <summary>
/// レイと AABB の交差判定
/// </summary>
bool RayIntersectAABB(const Ray& ray, const AABB& box, float* outT = nullptr);

/// <summary>
/// レイと OBB の交差判定
/// </summary>
bool RayIntersectOBB(const Ray& ray, const OBB& obb, float* outT = nullptr);

/// <summary>
/// レイと球の交差判定
/// </summary>
bool RayIntersectSphere(const Ray& ray, const Sphere& s, float* outT = nullptr);

// Transform から OBB を作成するヘルパー
// 引数 halfLengths はローカル空間における各軸の半長さ
OBB MakeOBBFromTransform(const Transform& t, const Vector3& halfLengths);

// Transform から AABB を作成するヘルパー
// 引数 halfLengths はローカル空間における各軸の半長さ
AABB MakeAABBFromTransform(const Transform& t, const Vector3& halfLengths);

// 衝突詳細情報を返すための構造体
struct CollisionResult {
    bool hit = false; // 衝突したか
    Vector3 point {}; // 接触点（近似）
    Vector3 normal {}; // 衝突法線（A -> B の向き）
    float penetration = 0.0f; // 貫入深度（正の値）
};

// レイの当たり情報
struct RayHitResult {
    bool hit = false; // ヒットしたか
    float t = 0.0f; // レイ長さ（パラメータ）
    Vector3 point {}; // ヒット位置
    Vector3 normal {}; // ヒット法線
};

// ----------------------
// 衝突判定詳細版の関数 - 衝突の有無だけでなく、接触点・法線・貫入深度も返す
// ----------------------

/// <summary>
/// 衝突判定関数（詳細版）
/// </summary>
CollisionResult IntersectAABB_AABB_Detailed(const AABB& a, const AABB& b);

/// <summary>
/// AABB と AABB の交差判定（詳細版）
/// </summary>
CollisionResult IntersectSphere_Sphere_Detailed(const Sphere& a, const Sphere& b);

/// <summary>
/// 球と球の交差判定（詳細版）
/// </summary>
CollisionResult IntersectAABB_Sphere_Detailed(const AABB& box, const Sphere& s);

/// <summary>
/// AABB と球の交差判定（詳細版）
/// </summary>
CollisionResult IntersectSphere_OBB_Detailed(const Sphere& s, const OBB& obb);

/// <summary>
/// OBB と OBB の交差判定（詳細版）
/// </summary>
CollisionResult IntersectOBB_OBB_Detailed(const OBB& A, const OBB& B);

//----------------------
// レイと衝突判定の詳細版 - 衝突の有無だけでなく、ヒット位置・法線・レイ長さも返す
//----------------------

/// <summary>
/// レイと AABB の交差判定（詳細版）
/// </summary>
RayHitResult RayIntersectAABB_Detailed(const Ray& ray, const AABB& box);

/// <summary>
/// レイと OBB の交差判定（詳細版）
/// </summary>
RayHitResult RayIntersectOBB_Detailed(const Ray& ray, const OBB& obb);

/// <summary>
/// レイと球の交差判定（詳細版）
/// </summary>
RayHitResult RayIntersectSphere_Detailed(const Ray& ray, const Sphere& s);

//----------------------
// 三角形とメッシュに対するレイキャスト関数
//----------------------

/// <summary>
/// レイと三角形の交差判定
/// </summary>
bool RayIntersectTriangle(const Ray& ray, const Triangle& tri, float* outT = nullptr, float* outU = nullptr, float* outV = nullptr);

/// <summary>
/// レイと三角形の交差判定（詳細版）
/// </summary>
RayHitResult RayIntersectTriangle_Detailed(const Ray& ray, const Triangle& tri);

/// <summary>
/// レイとメッシュの交差判定（最近接ヒットを返す）
/// </summary>
RayHitResult RayIntersectMesh(const Ray& ray, const Mesh& mesh);

// ----------------------
// 衝突フィルタ / レイヤー
// ----------------------

// 衝突フィルタリングのためのレイヤーとマスクの定義
using LayerMask = uint32_t;

/// <summary>
/// レイヤーマスクを作成するヘルパー関数
/// </summary>
inline LayerMask MakeLayerMask(int layer) { return (layer >= 0 && layer < 32) ? (1u << layer) : 0u; }

/// <summary>
/// 衝突判定関数で使用するレイヤーとマスクの組み合わせから、衝突すべきかを判定する関数
/// </summary>
inline bool ShouldCollide(LayerMask layerA, LayerMask collideMaskA, LayerMask layerB, LayerMask collideMaskB)
{
    return ((collideMaskA & layerB) != 0u) && ((collideMaskB & layerA) != 0u);
}

// エンティティに付与する Collider コンポーネントの定義
enum class ColliderType {
    AABB,
    OBB,
    Sphere,
    Mesh
};

/// Collider コンポーネント
struct Collider {
    ColliderType type = ColliderType::AABB;
    LayerMask layer = MakeLayerMask(0); // 所属レイヤー
    LayerMask collideMask = 0xFFFFFFFFu; // 衝突対象レイヤーのマスク（デフォルトは全て）
    // 形状データ（type に応じて使用）
    AABB aabb;
    OBB obb;
    Sphere sphere;
    Mesh mesh;
};

// ----------------------
// BVH（簡易実装）
// ----------------------

// BVH ノード
struct BVHNode {
    AABB bounds;
    int left = -1; // 左子ノードインデックス（-1 = なし）
    int right = -1; // 右子ノードインデックス（-1 = なし）
    int start = 0; // 三角形開始インデックス
    int count = 0; // 三角形数
};

// BVH クラス（メッシュに対する加速構造）
struct BVH {
    std::vector<BVHNode> nodes;
    std::vector<Triangle> triangles; // 再配置された三角形配列
    // コンストラクタでメッシュから BVH を構築する
    BVH() { }
    explicit BVH(const Mesh& mesh);
    // レイキャスト
    RayHitResult RayIntersect(const Ray& ray) const;
};

/// <summary>
/// レイとメッシュの交差判定（BVH ベース、最近接ヒットを返す）
/// </summary>
RayHitResult RayIntersectMesh_BVH(const Ray& ray, const Mesh& mesh);

// ----------------------
// 接触マニフォールド（簡易）
// ----------------------

// 接触マニフォールド - 複数の接触点をまとめる構造体
struct ContactPoint {
    Vector3 position;
    Vector3 normal; // A->B の向き
    float penetration = 0.0f;
};

// マニフォールド - 複数の接触点をまとめる構造体
struct ContactManifold {
    std::vector<ContactPoint> contacts;
};

// 便利関数 - マニフォールドに接触点を追加する

/// <summary>
/// 衝突判定関数（マニフォールド版）
/// </summary>
ContactManifold CreateManifoldAABB_AABB(const AABB& a, const AABB& b);

/// <summary>
/// AABB と AABB の交差判定（マニフォールド版）
/// </summary>
ContactManifold CreateManifoldSphere_Sphere(const Sphere& a, const Sphere& b);

/// <summary>
/// 球と球の交差判定（マニフォールド版）
/// </summary>
ContactManifold CreateManifoldSphere_OBB(const Sphere& s, const OBB& obb);

/// <summary>
/// AABB と球の交差判定（マニフォールド版）
/// </summary>
ContactManifold CreateManifoldOBB_OBB(const OBB& A, const OBB& B);

} // namespace CollisionUtility
