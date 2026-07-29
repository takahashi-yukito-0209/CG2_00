#pragma once

#include "CollisionUtility.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace MyEngine {

/// <summary>
/// 登録されたCollider同士の衝突判定結果を管理するクラス。
/// </summary>
class CollisionSystem {
public:
    /// <summary>
    /// 衝突判定へ登録するCollider参照。
    /// </summary>
    struct CollisionEntry {
        uint32_t objectId = 0; // 登録元オブジェクトを識別するID
        const CollisionUtility::Collider* collider = nullptr; // 判定に使用するCollider
        const void* owner = nullptr; // 呼び出し側が必要に応じて保持する所有元ポインタ
    };

    /// <summary>
    /// 衝突している2つのCollider情報。
    /// </summary>
    struct CollisionPair {
        uint32_t objectIdA = 0; // 片方の登録元ID
        uint32_t objectIdB = 0; // もう片方の登録元ID
        const CollisionUtility::Collider* colliderA = nullptr; // 片方のCollider
        const CollisionUtility::Collider* colliderB = nullptr; // もう片方のCollider
        const void* ownerA = nullptr; // 片方の所有元ポインタ
        const void* ownerB = nullptr; // もう片方の所有元ポインタ
    };

    /// <summary>
    /// レイキャストでヒットしたCollider情報。
    /// </summary>
    struct RaycastHit {
        uint32_t objectId = 0; // ヒットした登録元ID
        const CollisionUtility::Collider* collider = nullptr; // ヒットしたCollider
        const void* owner = nullptr; // ヒットした所有元ポインタ
        CollisionUtility::RayHitResult result; // レイのヒット詳細
    };

    /// <summary>
    /// 登録済みColliderと衝突結果をすべて消去する。
    /// </summary>
    void Clear();

    /// <summary>
    /// 判定対象のColliderを登録する。
    /// </summary>
    void RegisterCollider(uint32_t objectId, const CollisionUtility::Collider* collider, const void* owner = nullptr);

    /// <summary>
    /// 登録済みCollider同士の衝突判定を更新する。
    /// </summary>
    void Update();

    /// <summary>
    /// 固定グリッドによるブロードフェーズを使うか設定する。
    /// </summary>
    void SetSpatialHashEnabled(bool enabled) { useSpatialHash_ = enabled; }

    /// <summary>
    /// 固定グリッドのセルサイズを設定する。
    /// </summary>
    void SetSpatialHashCellSize(float cellSize);

    /// <summary>
    /// 固定グリッドによるブロードフェーズを使っているか取得する。
    /// </summary>
    bool GetSpatialHashEnabled() const { return useSpatialHash_; }

    /// <summary>
    /// 固定グリッドのセルサイズを取得する。
    /// </summary>
    float GetSpatialHashCellSize() const { return spatialHashCellSize_; }

    /// <summary>
    /// 前回更新で詳細判定へ進んだ候補ペア数を取得する。
    /// </summary>
    size_t GetLastCandidatePairCount() const { return lastCandidatePairCount_; }

    /// <summary>
    /// 衝突中のペア一覧を取得する。
    /// </summary>
    const std::vector<CollisionPair>& GetCollisionPairs() const { return collisionPairs_; }

    /// <summary>
    /// 登録中のCollider数を取得する。
    /// </summary>
    size_t GetColliderCount() const { return entries_.size(); }

    /// <summary>
    /// 指定IDのColliderが現在いずれかと衝突しているか取得する。
    /// </summary>
    bool HasCollision(uint32_t objectId) const;

    /// <summary>
    /// 指定IDに関係する最初の衝突ペアを取得する。
    /// </summary>
    const CollisionPair* FindFirstCollision(uint32_t objectId) const;

    /// <summary>
    /// 登録済みColliderに対して最近接レイキャストを行う。
    /// </summary>
    bool RaycastClosest(const CollisionUtility::Ray& ray, RaycastHit& outHit, CollisionUtility::LayerMask targetMask = 0xFFFFFFFFu, bool useMeshBvh = true) const;

private:
    /// <summary>
    /// 2つの登録情報を詳細判定し、衝突していれば結果へ追加する。
    /// </summary>
    void TestEntryPair(size_t entryIndexA, size_t entryIndexB);

    /// <summary>
    /// 登録順の総当たりで衝突判定を更新する。
    /// </summary>
    void UpdateBruteForce();

    /// <summary>
    /// 固定グリッドで候補を絞って衝突判定を更新する。
    /// </summary>
    void UpdateSpatialHash();

    std::vector<CollisionEntry> entries_; // 今フレームの判定対象
    std::vector<CollisionPair> collisionPairs_; // 今フレームの衝突結果
    bool useSpatialHash_ = true; // 固定グリッドで候補を絞るか
    float spatialHashCellSize_ = 4.0f; // 固定グリッドのセル一辺の長さ
    size_t lastCandidatePairCount_ = 0; // 前回更新で詳細判定した候補ペア数
};

} // namespace MyEngine
