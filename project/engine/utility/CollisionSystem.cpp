#include "CollisionSystem.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace MyEngine {
namespace {
constexpr float kMinimumSpatialHashCellSize = 0.01f; // 固定グリッドの最小セルサイズ
constexpr size_t kMaximumCellRegistrationPerCollider = 512; // 1Colliderが登録できる最大セル数

/// <summary>
/// 固定グリッドのセル座標。
/// </summary>
struct CellKey {
    int x = 0; // セルX座標
    int y = 0; // セルY座標
    int z = 0; // セルZ座標

    /// <summary>
    /// セル座標が同じか判定する。
    /// </summary>
    bool operator==(const CellKey& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

/// <summary>
/// セル座標をunordered_map用のハッシュ値へ変換する。
/// </summary>
struct CellKeyHash {
    size_t operator()(const CellKey& key) const
    {
        const size_t hashX = std::hash<int> {}(key.x); // X座標のハッシュ
        const size_t hashY = std::hash<int> {}(key.y); // Y座標のハッシュ
        const size_t hashZ = std::hash<int> {}(key.z); // Z座標のハッシュ
        return hashX ^ (hashY << 1) ^ (hashZ << 2);
    }
};

/// <summary>
/// 登録順インデックスのペア。
/// </summary>
struct EntryPairKey {
    size_t a = 0; // 小さい方の登録番号
    size_t b = 0; // 大きい方の登録番号

    /// <summary>
    /// ペアが同じか判定する。
    /// </summary>
    bool operator==(const EntryPairKey& other) const
    {
        return a == other.a && b == other.b;
    }
};

/// <summary>
/// 登録順インデックスのペアをunordered_set用のハッシュ値へ変換する。
/// </summary>
struct EntryPairKeyHash {
    size_t operator()(const EntryPairKey& key) const
    {
        const size_t hashA = std::hash<size_t> {}(key.a); // 片方の登録番号ハッシュ
        const size_t hashB = std::hash<size_t> {}(key.b); // もう片方の登録番号ハッシュ
        return hashA ^ (hashB << 1);
    }
};

/// <summary>
/// 値を固定グリッドのセル座標へ変換する。
/// </summary>
int ToCellIndex(float value, float cellSize)
{
    return static_cast<int>(std::floor(value / cellSize));
}

/// <summary>
/// AABBがまたぐセル数を取得する。
/// </summary>
size_t CountTouchedCells(const CellKey& minCell, const CellKey& maxCell)
{
    const size_t countX = static_cast<size_t>((std::max)(maxCell.x - minCell.x + 1, 1)); // X方向セル数
    const size_t countY = static_cast<size_t>((std::max)(maxCell.y - minCell.y + 1, 1)); // Y方向セル数
    const size_t countZ = static_cast<size_t>((std::max)(maxCell.z - minCell.z + 1, 1)); // Z方向セル数
    return countX * countY * countZ;
}

/// <summary>
/// 2つの登録番号から重複しないペアキーを作成する。
/// </summary>
EntryPairKey MakeEntryPairKey(size_t entryIndexA, size_t entryIndexB)
{
    EntryPairKey key {}; // 候補ペアを一意に扱うキー
    key.a = (std::min)(entryIndexA, entryIndexB);
    key.b = (std::max)(entryIndexA, entryIndexB);
    return key;
}
} // namespace

/// <summary>
/// 登録済みColliderと衝突結果をすべて消去する。
/// </summary>
void CollisionSystem::Clear()
{
    entries_.clear();
    collisionPairs_.clear();
    lastCandidatePairCount_ = 0;
}

/// <summary>
/// 判定対象のColliderを登録する。
/// </summary>
void CollisionSystem::RegisterCollider(uint32_t objectId, const CollisionUtility::Collider* collider, const void* owner)
{
    if (!collider) {
        return;
    }

    CollisionEntry entry {}; // 追加するCollider登録情報
    entry.objectId = objectId;
    entry.collider = collider;
    entry.owner = owner;
    entries_.push_back(entry);
}

/// <summary>
/// 登録済みCollider同士の衝突判定を更新する。
/// </summary>
void CollisionSystem::Update()
{
    collisionPairs_.clear();
    lastCandidatePairCount_ = 0;

    if (entries_.size() < 2) {
        return;
    }

    if (useSpatialHash_) {
        UpdateSpatialHash();
    } else {
        UpdateBruteForce();
    }
}

/// <summary>
/// 固定グリッドのセルサイズを設定する。
/// </summary>
void CollisionSystem::SetSpatialHashCellSize(float cellSize)
{
    spatialHashCellSize_ = (std::max)(cellSize, kMinimumSpatialHashCellSize);
}

/// <summary>
/// 2つの登録情報を詳細判定し、衝突していれば結果へ追加する。
/// </summary>
void CollisionSystem::TestEntryPair(size_t entryIndexA, size_t entryIndexB)
{
    if (entryIndexA == entryIndexB || entryIndexA >= entries_.size() || entryIndexB >= entries_.size()) {
        return;
    }

    const CollisionEntry& entryA = entries_[entryIndexA]; // 判定元の登録情報
    const CollisionEntry& entryB = entries_[entryIndexB]; // 判定先の登録情報
    if (!entryA.collider || !entryB.collider) {
        return;
    }

    lastCandidatePairCount_++;
    if (!CollisionUtility::IntersectCollider(*entryA.collider, *entryB.collider)) {
        return;
    }

    CollisionPair pair {}; // 衝突していた2つの登録情報
    pair.objectIdA = entryA.objectId;
    pair.objectIdB = entryB.objectId;
    pair.colliderA = entryA.collider;
    pair.colliderB = entryB.collider;
    pair.ownerA = entryA.owner;
    pair.ownerB = entryB.owner;
    collisionPairs_.push_back(pair);
}

/// <summary>
/// 登録順の総当たりで衝突判定を更新する。
/// </summary>
void CollisionSystem::UpdateBruteForce()
{
    for (size_t entryIndexA = 0; entryIndexA < entries_.size(); ++entryIndexA) {
        for (size_t entryIndexB = entryIndexA + 1; entryIndexB < entries_.size(); ++entryIndexB) {
            TestEntryPair(entryIndexA, entryIndexB);
        }
    }
}

/// <summary>
/// 固定グリッドで候補を絞って衝突判定を更新する。
/// </summary>
void CollisionSystem::UpdateSpatialHash()
{
    std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash> cellEntries; // セルごとの登録番号一覧
    cellEntries.reserve(entries_.size());

    for (size_t entryIndex = 0; entryIndex < entries_.size(); ++entryIndex) {
        const CollisionEntry& entry = entries_[entryIndex]; // セルへ登録するCollider情報
        if (!entry.collider) {
            continue;
        }

        const CollisionUtility::AABB bounds = CollisionUtility::GetColliderAABB(*entry.collider); // Colliderを内包するAABB
        const CellKey minCell {
            ToCellIndex(bounds.min.x, spatialHashCellSize_),
            ToCellIndex(bounds.min.y, spatialHashCellSize_),
            ToCellIndex(bounds.min.z, spatialHashCellSize_),
        }; // AABB最小側のセル座標
        const CellKey maxCell {
            ToCellIndex(bounds.max.x, spatialHashCellSize_),
            ToCellIndex(bounds.max.y, spatialHashCellSize_),
            ToCellIndex(bounds.max.z, spatialHashCellSize_),
        }; // AABB最大側のセル座標

        if (CountTouchedCells(minCell, maxCell) > kMaximumCellRegistrationPerCollider) {
            UpdateBruteForce();
            return;
        }

        for (int cellX = minCell.x; cellX <= maxCell.x; ++cellX) {
            for (int cellY = minCell.y; cellY <= maxCell.y; ++cellY) {
                for (int cellZ = minCell.z; cellZ <= maxCell.z; ++cellZ) {
                    const CellKey key { cellX, cellY, cellZ }; // 登録先セル
                    cellEntries[key].push_back(entryIndex);
                }
            }
        }
    }

    std::unordered_set<EntryPairKey, EntryPairKeyHash> testedPairs; // 詳細判定済みの候補ペア
    for (const auto& cellEntry : cellEntries) {
        const std::vector<size_t>& entryIndices = cellEntry.second; // 同じセルに入った登録番号一覧
        for (size_t localIndexA = 0; localIndexA < entryIndices.size(); ++localIndexA) {
            for (size_t localIndexB = localIndexA + 1; localIndexB < entryIndices.size(); ++localIndexB) {
                const EntryPairKey pairKey = MakeEntryPairKey(entryIndices[localIndexA], entryIndices[localIndexB]); // 重複排除用ペアキー
                if (!testedPairs.insert(pairKey).second) {
                    continue;
                }
                TestEntryPair(pairKey.a, pairKey.b);
            }
        }
    }
}

/// <summary>
/// 登録済みColliderに対して最近接レイキャストを行う。
/// </summary>
bool CollisionSystem::RaycastClosest(const CollisionUtility::Ray& ray, RaycastHit& outHit, CollisionUtility::LayerMask targetMask, bool useMeshBvh) const
{
    bool hasHit = false; // 1件以上ヒットしたか
    float nearestT = (std::numeric_limits<float>::max)(); // 現在見つかっている最近距離
    RaycastHit nearestHit {}; // 現在見つかっている最近ヒット

    for (const CollisionEntry& entry : entries_) {
        if (!entry.collider || (entry.collider->layer & targetMask) == 0u) {
            continue;
        }

        const CollisionUtility::RayHitResult result = CollisionUtility::RayIntersectCollider(ray, *entry.collider, useMeshBvh); // Colliderへのレイキャスト結果
        if (!result.hit || result.t >= nearestT) {
            continue;
        }

        hasHit = true;
        nearestT = result.t;
        nearestHit.objectId = entry.objectId;
        nearestHit.collider = entry.collider;
        nearestHit.owner = entry.owner;
        nearestHit.result = result;
    }

    if (hasHit) {
        outHit = nearestHit;
    }
    return hasHit;
}
/// <summary>
/// 指定IDのColliderが現在いずれかと衝突しているか取得する。
/// </summary>
bool CollisionSystem::HasCollision(uint32_t objectId) const
{
    return FindFirstCollision(objectId) != nullptr;
}

/// <summary>
/// 指定IDに関係する最初の衝突ペアを取得する。
/// </summary>
const CollisionSystem::CollisionPair* CollisionSystem::FindFirstCollision(uint32_t objectId) const
{
    for (const CollisionPair& pair : collisionPairs_) {
        if (pair.objectIdA == objectId || pair.objectIdB == objectId) {
            return &pair;
        }
    }

    return nullptr;
}

} // namespace MyEngine