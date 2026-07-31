#include "PlayScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/debug/DebugRenderer.h"
#include "../../engine/particle/ParticleManager.h"
#include "../../engine/particle/ParticleEmitter.h"
#include "../../engine/utility/mathUtility.h"
#include <algorithm>
#include <cmath>
#include <string>

using namespace MyEngine;

namespace {
constexpr Math::Vector3 kDebugGridCenter = { 0.0f, 0.0f, 0.0f }; // デバッググリッドの中心
constexpr Math::Vector4 kDebugGridColor = { 0.25f, 0.25f, 0.25f, 1.0f }; // デバッググリッドの色
constexpr int kDebugGridHalfLineCount = 20; // デバッググリッドの片側ライン数
constexpr float kDebugGridSpacing = 1.0f; // デバッググリッドの間隔
constexpr Math::Vector4 kHitEmitterDebugRangeColor = { 0.1f, 0.9f, 1.0f, 1.0f }; // Hitエミッター範囲表示の色
constexpr Math::Vector4 kHitEmitterDebugGridColor = { 0.1f, 0.65f, 1.0f, 0.65f }; // Hitエミッター範囲グリッドの色
constexpr Math::Vector4 kRingEmitterDebugRangeColor = { 1.0f, 0.45f, 0.9f, 1.0f }; // Ringエミッター範囲表示の色
constexpr Math::Vector4 kRingEmitterDebugGridColor = { 1.0f, 0.3f, 0.75f, 0.65f }; // Ringエミッター範囲グリッドの色
constexpr Math::Vector4 kCylinderEmitterDebugRangeColor = { 1.0f, 0.85f, 0.15f, 1.0f }; // Cylinderエミッター範囲表示の色
constexpr Math::Vector4 kCylinderEmitterDebugGridColor = { 1.0f, 0.7f, 0.1f, 0.65f }; // Cylinderエミッター範囲グリッドの色
constexpr Math::Vector4 kGpuEmitterDebugRangeColor = { 0.75f, 0.35f, 1.0f, 1.0f }; // GPU Emitter範囲表示の色
constexpr Math::Vector4 kGpuEmitterDebugGridColor = { 0.6f, 0.25f, 1.0f, 0.65f }; // GPU Emitter補助線の色
constexpr Math::Vector4 kLevelColliderDebugColor = { 0.2f, 1.0f, 0.95f, 1.0f }; // レベルコライダーの表示色
constexpr Math::Vector4 kLevelColliderHitDebugColor = { 1.0f, 0.25f, 0.2f, 1.0f }; // 衝突中コライダーの表示色
constexpr Math::Vector4 kLevelSpawnPointDebugColor = { 0.2f, 1.0f, 0.35f, 1.0f }; // スポーン地点の表示色
constexpr Math::Vector4 kLevelEventTriggerDebugColor = { 1.0f, 0.85f, 0.2f, 1.0f }; // イベントトリガーの表示色
constexpr Math::Vector4 kLevelCameraStartDebugColor = { 0.35f, 0.6f, 1.0f, 1.0f }; // 開始カメラの表示色
constexpr uint32_t kGpuSpawnShapeSphere = 0; // GPU Emitterの球形状番号
constexpr uint32_t kGpuSpawnShapeBox = 1; // GPU Emitterの箱形状番号
constexpr uint32_t kGpuSpawnShapeRing = 2; // GPU Emitterのリング形状番号
constexpr uint32_t kGpuSpawnShapeCone = 3; // GPU Emitterのコーン形状番号
constexpr int kGpuEmitterCircleSegmentCount = 48; // GPU Emitter円形ワイヤーの分割数
constexpr int kLevelColliderCircleSegmentCount = 48; // レベルコライダー円形ワイヤーの分割数

/// <summary>
/// Vector3同士の外積を取得する。
/// </summary>
Math::Vector3 CrossVector3(const Math::Vector3& a, const Math::Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

/// <summary>
/// ローカル座標をエミッターのSRTでワールド座標へ変換する。
/// </summary>
Math::Vector3 TransformEmitterDebugPoint(const ParticleEmitter& emitter, const Math::Vector3& localPosition)
{
    const Math::Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(emitter.transform.scale, emitter.transform.rotate, emitter.transform.translate); // エミッターのSRT行列
    return MathUtil::Transform(localPosition, worldMatrix);
}

/// <summary>
/// GPU Emitterのローカル座標をワールド座標へ変換する。
/// </summary>
Math::Vector3 TransformGpuEmitterDebugPoint(const PM_GpuEmitterSphere& emitter, const Math::Vector3& localPosition)
{
    const float radius = (std::max)(emitter.radius, 0.01f); // 表示に使う発生半径
    return emitter.translate + localPosition * radius;
}

/// <summary>
/// GPU Emitter範囲の円ワイヤーを描画する。
/// </summary>
void DrawGpuEmitterDebugCircle(DebugRenderer& debugRenderer, const PM_GpuEmitterSphere& emitter, const Math::Vector3& center, const Math::Vector3& axisA, const Math::Vector3& axisB, const Math::Vector4& color)
{
    Math::Vector3 previousPoint = TransformGpuEmitterDebugPoint(emitter, center + axisA); // 直前の円周点
    for (int segmentIndex = 1; segmentIndex <= kGpuEmitterCircleSegmentCount; ++segmentIndex) {
        const float rate = static_cast<float>(segmentIndex) / static_cast<float>(kGpuEmitterCircleSegmentCount); // 円周上の割合
        const float angle = rate * 6.28318530718f; // 円周角
        const float cosValue = std::cos(angle); // 円周上の横成分
        const float sinValue = std::sin(angle); // 円周上の縦成分
        const Math::Vector3 currentPoint = TransformGpuEmitterDebugPoint(emitter, center + axisA * cosValue + axisB * sinValue); // 現在の円周点
        debugRenderer.DrawLine3D(previousPoint, currentPoint, color);
        previousPoint = currentPoint;
    }
}

/// <summary>
/// GPU Emitter範囲の箱ワイヤーを描画する。
/// </summary>
void DrawGpuEmitterDebugBox(DebugRenderer& debugRenderer, const PM_GpuEmitterSphere& emitter, const Math::Vector4& color)
{
    const Math::Vector3 corners[] = {
        { -1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, 1.0f },
        { -1.0f, -1.0f, 1.0f },
        { -1.0f, 1.0f, -1.0f },
        { 1.0f, 1.0f, -1.0f },
        { 1.0f, 1.0f, 1.0f },
        { -1.0f, 1.0f, 1.0f },
    }; // 箱のローカル八隅
    const int edges[][2] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    }; // 箱の辺
    for (const auto& edge : edges) {
        debugRenderer.DrawLine3D(
            TransformGpuEmitterDebugPoint(emitter, corners[edge[0]]),
            TransformGpuEmitterDebugPoint(emitter, corners[edge[1]]),
            color);
    }
}

/// <summary>
/// GPU Emitter範囲のリングワイヤーを描画する。
/// </summary>
void DrawGpuEmitterDebugRing(DebugRenderer& debugRenderer, const PM_GpuEmitterSphere& emitter, const Math::Vector4& rangeColor, const Math::Vector4& gridColor)
{
    constexpr float ringHalfHeight = 0.05f; // シェーダー側リング高さの半分
    DrawGpuEmitterDebugCircle(debugRenderer, emitter, { 0.0f, -ringHalfHeight, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, gridColor);
    DrawGpuEmitterDebugCircle(debugRenderer, emitter, { 0.0f, ringHalfHeight, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, rangeColor);
    for (int segmentIndex = 0; segmentIndex < 8; ++segmentIndex) {
        const float angle = static_cast<float>(segmentIndex) / 8.0f * 6.28318530718f; // 補助線の角度
        const float cosValue = std::cos(angle); // リング外周のX成分
        const float sinValue = std::sin(angle); // リング外周のZ成分
        const Math::Vector3 lowerPoint = { cosValue, -ringHalfHeight, sinValue }; // 下側リング外周点
        const Math::Vector3 upperPoint = { cosValue, ringHalfHeight, sinValue }; // 上側リング外周点
        debugRenderer.DrawLine3D(
            TransformGpuEmitterDebugPoint(emitter, lowerPoint),
            TransformGpuEmitterDebugPoint(emitter, upperPoint),
            gridColor);
    }
}

/// <summary>
/// GPU Emitter範囲のコーンワイヤーを描画する。
/// </summary>
void DrawGpuEmitterDebugCone(DebugRenderer& debugRenderer, const PM_GpuEmitterSphere& emitter, const Math::Vector4& rangeColor, const Math::Vector4& gridColor)
{
    DrawGpuEmitterDebugCircle(debugRenderer, emitter, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, rangeColor);
    const Math::Vector3 apex = { 0.0f, 1.0f, 0.0f }; // コーン頂点
    for (int segmentIndex = 0; segmentIndex < 8; ++segmentIndex) {
        const float angle = static_cast<float>(segmentIndex) / 8.0f * 6.28318530718f; // 側面線の角度
        const float cosValue = std::cos(angle); // 底面上のX成分
        const float sinValue = std::sin(angle); // 底面上のZ成分
        const Math::Vector3 basePoint = { cosValue, 0.0f, sinValue }; // 底面上の点
        debugRenderer.DrawLine3D(
            TransformGpuEmitterDebugPoint(emitter, basePoint),
            TransformGpuEmitterDebugPoint(emitter, apex),
            gridColor);
    }
}

/// <summary>
/// GPU Emitterのデバッグ範囲を描画する。
/// </summary>
void DrawGpuEmitterDebugRange(DebugRenderer& debugRenderer, const PM_GpuEmitterSphere& emitter)
{
    const uint32_t spawnShape = (std::min)(emitter.spawnShape, kGpuSpawnShapeCone); // 表示に使う発生形状
    if (spawnShape == kGpuSpawnShapeBox) {
        DrawGpuEmitterDebugBox(debugRenderer, emitter, kGpuEmitterDebugRangeColor);
    } else if (spawnShape == kGpuSpawnShapeRing) {
        DrawGpuEmitterDebugRing(debugRenderer, emitter, kGpuEmitterDebugRangeColor, kGpuEmitterDebugGridColor);
    } else if (spawnShape == kGpuSpawnShapeCone) {
        DrawGpuEmitterDebugCone(debugRenderer, emitter, kGpuEmitterDebugRangeColor, kGpuEmitterDebugGridColor);
    } else {
        DrawGpuEmitterDebugCircle(debugRenderer, emitter, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, kGpuEmitterDebugGridColor);
        DrawGpuEmitterDebugCircle(debugRenderer, emitter, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, kGpuEmitterDebugRangeColor);
        DrawGpuEmitterDebugCircle(debugRenderer, emitter, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, kGpuEmitterDebugGridColor);
    }

    const float markerLength = 0.35f; // 中心マーカーのローカル長さ
    debugRenderer.DrawLine3D(TransformGpuEmitterDebugPoint(emitter, { -markerLength, 0.0f, 0.0f }), TransformGpuEmitterDebugPoint(emitter, { markerLength, 0.0f, 0.0f }), { 1.0f, 0.2f, 0.2f, 1.0f });
    debugRenderer.DrawLine3D(TransformGpuEmitterDebugPoint(emitter, { 0.0f, -markerLength, 0.0f }), TransformGpuEmitterDebugPoint(emitter, { 0.0f, markerLength, 0.0f }), { 0.2f, 1.0f, 0.2f, 1.0f });
    debugRenderer.DrawLine3D(TransformGpuEmitterDebugPoint(emitter, { 0.0f, 0.0f, -markerLength }), TransformGpuEmitterDebugPoint(emitter, { 0.0f, 0.0f, markerLength }), { 0.2f, 0.45f, 1.0f, 1.0f });
}

/// <summary>
/// エミッターのデバッグ範囲グリッドを描画する。
/// </summary>
void DrawEmitterDebugGrid(DebugRenderer& debugRenderer, const ParticleEmitter& emitter, const Math::Vector4& rangeColor, const Math::Vector4& gridColor)
{
    if (!emitter.showDebugRange) {
        return;
    }

    const Math::Vector3 halfSize = {
        (std::max)(emitter.debugRangeHalfSize.x, 0.01f),
        (std::max)(emitter.debugRangeHalfSize.y, 0.01f),
        (std::max)(emitter.debugRangeHalfSize.z, 0.01f)
    }; // 表示する範囲の半径
    const int halfLineCount = (std::max)(emitter.debugGridHalfLineCount, 1); // グリッド片側ライン数
    const float spacing = (std::max)(emitter.debugGridSpacing, 0.01f); // グリッド間隔
    const float maxX = (std::max)(halfSize.x, spacing * static_cast<float>(halfLineCount)); // X方向の最大幅
    const float maxY = halfSize.y; // Y方向の最大幅
    const float maxZ = (std::max)(halfSize.z, spacing * static_cast<float>(halfLineCount)); // Z方向の最大幅
    const float gridY = 0.03f; // 地面グリッドと重ならないように少し浮かせる高さ

    for (int lineIndex = -halfLineCount; lineIndex <= halfLineCount; ++lineIndex) {
        const float x = (std::clamp)(static_cast<float>(lineIndex) * spacing, -maxX, maxX); // X方向グリッド位置
        const float z = (std::clamp)(static_cast<float>(lineIndex) * spacing, -maxZ, maxZ); // Z方向グリッド位置
        debugRenderer.DrawLine3D(
            TransformEmitterDebugPoint(emitter, { x, gridY, -maxZ }),
            TransformEmitterDebugPoint(emitter, { x, gridY, maxZ }),
            gridColor);
        debugRenderer.DrawLine3D(
            TransformEmitterDebugPoint(emitter, { -maxX, gridY, z }),
            TransformEmitterDebugPoint(emitter, { maxX, gridY, z }),
            gridColor);
    }

    const Math::Vector3 corners[] = {
        { -maxX, -maxY, -maxZ },
        { maxX, -maxY, -maxZ },
        { maxX, -maxY, maxZ },
        { -maxX, -maxY, maxZ },
        { -maxX, maxY, -maxZ },
        { maxX, maxY, -maxZ },
        { maxX, maxY, maxZ },
        { -maxX, maxY, maxZ },
    }; // 範囲箱のローカル八隅
    const int edges[][2] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    }; // 範囲箱の辺
    for (const auto& edge : edges) {
        debugRenderer.DrawLine3D(
            TransformEmitterDebugPoint(emitter, corners[edge[0]]),
            TransformEmitterDebugPoint(emitter, corners[edge[1]]),
            rangeColor);
    }

    const float markerLength = (std::min)((std::min)(maxX, maxZ), 0.35f); // 中心マーカーの長さ
    debugRenderer.DrawLine3D(TransformEmitterDebugPoint(emitter, { -markerLength, 0.0f, 0.0f }), TransformEmitterDebugPoint(emitter, { markerLength, 0.0f, 0.0f }), { 1.0f, 0.2f, 0.2f, 1.0f });
    debugRenderer.DrawLine3D(TransformEmitterDebugPoint(emitter, { 0.0f, -markerLength, 0.0f }), TransformEmitterDebugPoint(emitter, { 0.0f, markerLength, 0.0f }), { 0.2f, 1.0f, 0.2f, 1.0f });
    debugRenderer.DrawLine3D(TransformEmitterDebugPoint(emitter, { 0.0f, 0.0f, -markerLength }), TransformEmitterDebugPoint(emitter, { 0.0f, 0.0f, markerLength }), { 0.2f, 0.45f, 1.0f, 1.0f });
}

/// <summary>
/// ワールド座標の指定2軸平面へ円形ワイヤーを追加する。
/// </summary>
void DrawWorldCircleRing(DebugRenderer& debugRenderer, const Math::Vector3& center, const Math::Vector3& axisA, float radiusA, const Math::Vector3& axisB, float radiusB, const Math::Vector4& color)
{
    const float safeRadiusA = (std::max)(std::fabs(radiusA), 0.001f); // 1つ目の軸方向の半径
    const float safeRadiusB = (std::max)(std::fabs(radiusB), 0.001f); // 2つ目の軸方向の半径
    Math::Vector3 previousPoint = center + axisA * safeRadiusA; // 直前の円周点

    for (int segmentIndex = 1; segmentIndex <= kLevelColliderCircleSegmentCount; ++segmentIndex) {
        const float rate = static_cast<float>(segmentIndex) / static_cast<float>(kLevelColliderCircleSegmentCount); // 円周上の割合
        const float angle = rate * 6.28318530718f; // 円周角
        const float cosValue = std::cos(angle); // 円周上の1軸目成分
        const float sinValue = std::sin(angle); // 円周上の2軸目成分
        const Math::Vector3 currentPoint = center + axisA * (cosValue * safeRadiusA) + axisB * (sinValue * safeRadiusB); // 現在の円周点
        debugRenderer.DrawLine3D(previousPoint, currentPoint, color);
        previousPoint = currentPoint;
    }
}

/// <summary>
/// ワールド座標の球ワイヤーを追加する。
/// </summary>
void DrawWorldSphere(DebugRenderer& debugRenderer, const Math::Vector3& center, float radius, const Math::Vector4& color)
{
    DrawWorldCircleRing(debugRenderer, center, { 1.0f, 0.0f, 0.0f }, radius, { 0.0f, 1.0f, 0.0f }, radius, color);
    DrawWorldCircleRing(debugRenderer, center, { 1.0f, 0.0f, 0.0f }, radius, { 0.0f, 0.0f, 1.0f }, radius, color);
    DrawWorldCircleRing(debugRenderer, center, { 0.0f, 1.0f, 0.0f }, radius, { 0.0f, 0.0f, 1.0f }, radius, color);
}

/// <summary>
/// ワールド座標のカプセルワイヤーを追加する。
/// </summary>
void DrawWorldCapsule(DebugRenderer& debugRenderer, const CollisionUtility::Capsule& capsule, const Math::Vector4& color)
{
    const float radius = (std::max)(capsule.radius, 0.001f); // カプセル半径
    const Math::Vector3 segment = capsule.end - capsule.start; // カプセル軸線分
    const float segmentLength = std::sqrt(segment.x * segment.x + segment.y * segment.y + segment.z * segment.z); // 軸線分の長さ
    const Math::Vector3 axis = MathUtil::SafeNormalize(segment, { 0.0f, 1.0f, 0.0f }); // カプセル軸方向
    const Math::Vector3 referenceAxis = std::fabs(axis.y) < 0.9f ? Math::Vector3 { 0.0f, 1.0f, 0.0f } : Math::Vector3 { 1.0f, 0.0f, 0.0f }; // 断面軸を作る参照軸
    const Math::Vector3 sideAxis = MathUtil::SafeNormalize(CrossVector3(axis, referenceAxis), { 1.0f, 0.0f, 0.0f }); // 断面の横軸
    const Math::Vector3 upAxis = MathUtil::SafeNormalize(CrossVector3(sideAxis, axis), { 0.0f, 0.0f, 1.0f }); // 断面の縦軸
    const Math::Vector3 center = (capsule.start + capsule.end) * 0.5f; // カプセル中心
    const float axisRadius = segmentLength * 0.5f + radius; // 端球を含む軸方向半径

    DrawWorldCircleRing(debugRenderer, capsule.start, sideAxis, radius, upAxis, radius, color);
    DrawWorldCircleRing(debugRenderer, capsule.end, sideAxis, radius, upAxis, radius, color);
    DrawWorldCircleRing(debugRenderer, center, sideAxis, radius, axis, axisRadius, color);
    DrawWorldCircleRing(debugRenderer, center, upAxis, radius, axis, axisRadius, color);

    debugRenderer.DrawLine3D(capsule.start + sideAxis * radius, capsule.end + sideAxis * radius, color);
    debugRenderer.DrawLine3D(capsule.start - sideAxis * radius, capsule.end - sideAxis * radius, color);
    debugRenderer.DrawLine3D(capsule.start + upAxis * radius, capsule.end + upAxis * radius, color);
    debugRenderer.DrawLine3D(capsule.start - upAxis * radius, capsule.end - upAxis * radius, color);
}

/// <summary>
/// Object3dが保持しているコライダーをデバッグ描画へ追加する。
/// </summary>
void DrawSceneObjectCollider(DebugRenderer& debugRenderer, const Object3d& object3d, bool isColliding)
{
    if (!object3d.HasCollider()) {
        return;
    }

    const CollisionUtility::Collider& collider = object3d.GetCollider(); // 描画対象のコライダー
    const Math::Vector4& colliderColor = isColliding ? kLevelColliderHitDebugColor : kLevelColliderDebugColor; // 衝突状態に応じた表示色
    if (collider.type == CollisionUtility::ColliderType::OBB) {
        debugRenderer.DrawOBB(collider.obb, colliderColor);
    } else if (collider.type == CollisionUtility::ColliderType::AABB) {
        debugRenderer.DrawAABB(collider.aabb.min, collider.aabb.max, colliderColor);
    } else if (collider.type == CollisionUtility::ColliderType::Sphere) {
        DrawWorldSphere(debugRenderer, collider.sphere.center, collider.sphere.radius, colliderColor);
    } else if (collider.type == CollisionUtility::ColliderType::Capsule) {
        DrawWorldCapsule(debugRenderer, collider.capsule, colliderColor);
    }
}

/// <summary>
/// LevelObjectDataのローカル座標をワールド座標へ変換する。
/// </summary>
Math::Vector3 TransformLevelColliderPoint(const LevelObjectData& objectData, const Math::Vector3& localPosition)
{
    const Math::Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(objectData.transform.scale, objectData.transform.rotate, objectData.transform.translate); // LevelObjectのワールド行列
    return MathUtil::Transform(localPosition, worldMatrix);
}

/// <summary>
/// LevelObjectDataのBOXコライダーをデバッグ描画へ追加する。
/// </summary>
void DrawLevelColliderBox(DebugRenderer& debugRenderer, const LevelObjectData& objectData, const Math::Vector4& color)
{
    const Math::Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(objectData.transform.scale, objectData.transform.rotate, objectData.transform.translate); // LevelObjectのワールド行列
    const Math::Vector3 worldCenter = MathUtil::Transform(objectData.collider.center, worldMatrix); // コライダー中心のワールド座標
    const Math::Vector3 halfLengths = {
        objectData.collider.size.x * 0.5f,
        objectData.collider.size.y * 0.5f,
        objectData.collider.size.z * 0.5f
    }; // コライダーの半サイズ
    Math::Transform colliderTransform = objectData.transform; // コライダー形状へ反映するTransform
    colliderTransform.translate = worldCenter;

    const CollisionUtility::OBB colliderObb = CollisionUtility::MakeOBBFromTransform(colliderTransform, halfLengths); // 表示用OBB
    debugRenderer.DrawOBB(colliderObb, color);
}

/// <summary>
/// LevelObjectDataのSPHEREコライダー半径をワールド基準で計算する。
/// </summary>
float CalculateLevelColliderSphereRadius(const LevelObjectData& objectData)
{
    const float diameterX = std::fabs(objectData.collider.size.x * objectData.transform.scale.x); // X方向のワールド直径
    const float diameterY = std::fabs(objectData.collider.size.y * objectData.transform.scale.y); // Y方向のワールド直径
    const float diameterZ = std::fabs(objectData.collider.size.z * objectData.transform.scale.z); // Z方向のワールド直径
    const float diameter = (std::max)((std::max)(diameterX, diameterY), diameterZ); // 球として扱う最大直径
    return (std::max)(diameter * 0.5f, 0.001f);
}

/// <summary>
/// LevelObjectDataのSPHEREコライダーをデバッグ描画へ追加する。
/// </summary>
void DrawLevelColliderSphere(DebugRenderer& debugRenderer, const LevelObjectData& objectData, const Math::Vector4& color)
{
    const Math::Vector3 worldCenter = TransformLevelColliderPoint(objectData, objectData.collider.center); // コライダー中心のワールド座標
    const float radius = CalculateLevelColliderSphereRadius(objectData); // 実体判定と同じ球半径
    DrawWorldSphere(debugRenderer, worldCenter, radius, color);
}

/// <summary>
/// LevelObjectDataのCAPSULEコライダーをワールド基準へ変換する。
/// </summary>
CollisionUtility::Capsule BuildLevelColliderCapsule(const LevelObjectData& objectData)
{
    const Math::Vector3 worldCenter = TransformLevelColliderPoint(objectData, objectData.collider.center); // コライダー中心のワールド座標
    const float diameterX = std::fabs(objectData.collider.size.x * objectData.transform.scale.x); // X方向のワールド直径
    const float diameterZ = std::fabs(objectData.collider.size.z * objectData.transform.scale.z); // Z方向のワールド直径
    const float radius = (std::max)((std::max)(diameterX, diameterZ) * 0.5f, 0.001f); // カプセル半径
    const float totalHeight = (std::max)(std::fabs(objectData.collider.size.y * objectData.transform.scale.y), radius * 2.0f); // カプセル全高
    const float segmentHalfLength = (std::max)(totalHeight * 0.5f - radius, 0.0f); // 球端を除いた軸の半分長さ
    const Math::Matrix4x4 rotateMatrix = MathUtil::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, objectData.transform.rotate, { 0.0f, 0.0f, 0.0f }); // 回転だけを反映する行列
    const Math::Vector3 capsuleAxis = MathUtil::SafeNormalize(MathUtil::Transform({ 0.0f, 1.0f, 0.0f }, rotateMatrix), { 0.0f, 1.0f, 0.0f }); // カプセル軸のワールド方向
    CollisionUtility::Capsule capsule {}; // 表示に使うカプセル
    capsule.start = worldCenter - capsuleAxis * segmentHalfLength;
    capsule.end = worldCenter + capsuleAxis * segmentHalfLength;
    capsule.radius = radius;
    return capsule;
}

/// <summary>
/// LevelObjectDataのCAPSULEコライダーをデバッグ描画へ追加する。
/// </summary>
void DrawLevelColliderCapsule(DebugRenderer& debugRenderer, const LevelObjectData& objectData, const Math::Vector4& color)
{
    const CollisionUtility::Capsule capsule = BuildLevelColliderCapsule(objectData); // 実体判定と同じカプセル
    DrawWorldCapsule(debugRenderer, capsule, color);
}

/// <summary>
/// LevelObjectDataのスポーン地点マーカーをデバッグ描画へ追加する。
/// </summary>
void DrawLevelSpawnPointMarker(DebugRenderer& debugRenderer, const LevelObjectData& objectData)
{
    constexpr float kMarkerLength = 0.45f; // スポーン地点マーカーの長さ
    const Math::Vector3 center = objectData.transform.translate; // スポーン地点のワールド座標
    debugRenderer.DrawLine3D(center + Math::Vector3 { -kMarkerLength, 0.0f, 0.0f }, center + Math::Vector3 { kMarkerLength, 0.0f, 0.0f }, kLevelSpawnPointDebugColor);
    debugRenderer.DrawLine3D(center + Math::Vector3 { 0.0f, -kMarkerLength, 0.0f }, center + Math::Vector3 { 0.0f, kMarkerLength, 0.0f }, kLevelSpawnPointDebugColor);
    debugRenderer.DrawLine3D(center + Math::Vector3 { 0.0f, 0.0f, -kMarkerLength }, center + Math::Vector3 { 0.0f, 0.0f, kMarkerLength }, kLevelSpawnPointDebugColor);
}

/// <summary>
/// LevelObjectDataのイベントトリガー範囲をデバッグ描画へ追加する。
/// </summary>
void DrawLevelEventTrigger(DebugRenderer& debugRenderer, const LevelObjectData& objectData)
{
    Math::Transform triggerTransform = objectData.transform; // イベントトリガー表示に使うTransform
    const Math::Vector3 halfLengths = {
        objectData.eventTrigger.size.x * 0.5f,
        objectData.eventTrigger.size.y * 0.5f,
        objectData.eventTrigger.size.z * 0.5f
    }; // イベントトリガーの半サイズ
    const CollisionUtility::OBB triggerObb = CollisionUtility::MakeOBBFromTransform(triggerTransform, halfLengths); // 表示用OBB
    debugRenderer.DrawOBB(triggerObb, kLevelEventTriggerDebugColor);
}

/// <summary>
/// LevelObjectDataの開始カメラマーカーをデバッグ描画へ追加する。
/// </summary>
void DrawLevelCameraStartMarker(DebugRenderer& debugRenderer, const LevelObjectData& objectData)
{
    constexpr float kForwardLength = 0.8f; // カメラ前方向マーカーの長さ
    constexpr float kSideLength = 0.3f; // カメラ横方向マーカーの長さ
    const Math::Matrix4x4 rotateMatrix = MathUtil::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, objectData.transform.rotate, { 0.0f, 0.0f, 0.0f }); // カメラ回転行列
    const Math::Vector3 center = objectData.transform.translate; // 開始カメラのワールド座標
    const Math::Vector3 forward = MathUtil::SafeNormalize(MathUtil::Transform({ 0.0f, 0.0f, 1.0f }, rotateMatrix), { 0.0f, 0.0f, 1.0f }); // カメラ前方向
    debugRenderer.DrawLine3D(center, center + forward * kForwardLength, kLevelCameraStartDebugColor);
    debugRenderer.DrawLine3D(center + Math::Vector3 { -kSideLength, 0.0f, 0.0f }, center + Math::Vector3 { kSideLength, 0.0f, 0.0f }, kLevelCameraStartDebugColor);
    debugRenderer.DrawLine3D(center + Math::Vector3 { 0.0f, -kSideLength, 0.0f }, center + Math::Vector3 { 0.0f, kSideLength, 0.0f }, kLevelCameraStartDebugColor);
}

/// <summary>
/// LevelObjectDataのゲーム用メタ情報をデバッグ描画へ追加する。
/// </summary>
void DrawLevelObjectMetadata(DebugRenderer& debugRenderer, const LevelObjectData& objectData)
{
    if (objectData.spawnPoint) {
        DrawLevelSpawnPointMarker(debugRenderer, objectData);
    }
    if (objectData.eventTrigger.enabled) {
        DrawLevelEventTrigger(debugRenderer, objectData);
    }
    if (objectData.cameraStart) {
        DrawLevelCameraStartMarker(debugRenderer, objectData);
    }
}

/// <summary>
/// LevelObjectDataのコライダーを種別に合わせてデバッグ描画へ追加する。
/// </summary>
void DrawLevelObjectCollider(DebugRenderer& debugRenderer, const LevelObjectData& objectData)
{
    if (!objectData.enabled) {
        return;
    }

    DrawLevelObjectMetadata(debugRenderer, objectData);
    if (objectData.collider.enabled) {
        if (objectData.collider.type == "SPHERE") {
            DrawLevelColliderSphere(debugRenderer, objectData, kLevelColliderDebugColor);
        } else if (objectData.collider.type == "CAPSULE") {
            DrawLevelColliderCapsule(debugRenderer, objectData, kLevelColliderDebugColor);
        } else {
            DrawLevelColliderBox(debugRenderer, objectData, kLevelColliderDebugColor);
        }
    }

    for (const LevelObjectData& childObjectData : objectData.children) {
        DrawLevelObjectCollider(debugRenderer, childObjectData);
    }
}

/// <summary>
/// LevelData階層内のコライダーをデバッグ描画へ追加する。
/// </summary>
void DrawLevelObjectColliders(DebugRenderer& debugRenderer, const std::vector<LevelObjectData>& objectDataList)
{
    for (const LevelObjectData& objectData : objectDataList) {
        DrawLevelObjectCollider(debugRenderer, objectData);
    }
}

/// <summary>
/// LevelData階層内のゲーム用メタ情報だけをデバッグ描画へ追加する。
/// </summary>
void DrawLevelObjectMetadataMarkers(DebugRenderer& debugRenderer, const std::vector<LevelObjectData>& objectDataList)
{
    for (const LevelObjectData& objectData : objectDataList) {
        if (!objectData.enabled) {
            continue;
        }
        DrawLevelObjectMetadata(debugRenderer, objectData);
        if (!objectData.children.empty()) {
            DrawLevelObjectMetadataMarkers(debugRenderer, objectData.children);
        }
    }
}
}

/// <summary>
/// シーン内の 3D 要素を描画する。
/// </summary>
void PlayScene::DrawSceneContent()
{
    DrawWorldAndParticles();
}

/// <summary>
/// 登録済みの3Dオブジェクトを描画する。
/// </summary>
void PlayScene::DrawSceneObjects()
{
    for (auto& object3d : objects3d_) {
        if (object3d) {
            object3d->Draw();
        }
    }
}

/// <summary>
/// 登録済み3Dモデル、パーティクル、グリッドを描画する。
/// </summary>
void PlayScene::DrawWorldAndParticles()
{
    if (ctx_.object3dCommon) {
        ctx_.object3dCommon->SetCommonDrawSetting();
        DrawSceneObjects();
    }

    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル描画を担当する管理クラス
    if (!kUsePostEffectPreviewScene && particleManager) {
        particleManager->Draw();
    }

    if (ctx_.debugRenderer) {
        if (!kUsePostEffectPreviewScene) {
            ctx_.debugRenderer->DrawGrid(kDebugGridCenter, kDebugGridHalfLineCount, kDebugGridSpacing, kDebugGridColor);
        }
        if (!levelData_.objects.empty()) {
            DrawLevelObjectMetadataMarkers(*ctx_.debugRenderer, levelData_.objects);
        }
        for (const auto& object3d : objects3d_) {
            if (object3d) {
                DrawSceneObjectCollider(*ctx_.debugRenderer, *object3d, collisionSystem_.HasCollision(object3d->GetObjectId()));
            }
        }
        if (!kUsePostEffectPreviewScene) {
            DrawEmitterDebugGrid(*ctx_.debugRenderer, pmEmitter_, kHitEmitterDebugRangeColor, kHitEmitterDebugGridColor);
            DrawEmitterDebugGrid(*ctx_.debugRenderer, ringEmitter_, kRingEmitterDebugRangeColor, kRingEmitterDebugGridColor);
            DrawEmitterDebugGrid(*ctx_.debugRenderer, cylinderEmitter_, kCylinderEmitterDebugRangeColor, kCylinderEmitterDebugGridColor);
            if (particleManager && particleManager->GetGpuEmitterState()) {
                DrawGpuEmitterDebugRange(*ctx_.debugRenderer, *particleManager->GetGpuEmitterState());
            }
        }
    }
}

/// <summary>
/// ポストプロセスの影響を受けないスプライトを描画する。
/// </summary>
void PlayScene::DrawSprites()
{
    if (kUsePostEffectPreviewScene || !ctx_.spriteCommon) {
        return;
    }

    ctx_.spriteCommon->SetCommonDrawSetting();
    for (auto& sprite : sprites_) {
        if (sprite) {
            sprite->Draw();
        }
    }
}

/// <summary>
/// 蓄積した 3D デバッグラインを現在の描画先へ描画する。
/// </summary>
void PlayScene::DrawDebugLines3D()
{
    if (!ctx_.debugRenderer || !ctx_.camera) {
        return;
    }

    const Math::Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // デバッグライン描画に使用するビュー行列
    const Math::Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // デバッグライン描画に使用する射影行列
    ctx_.debugRenderer->Render3D(viewMatrix, projectionMatrix);
}

/// <summary>
/// 蓄積した 2D デバッグラインを現在の描画先へ描画する。
/// </summary>
void PlayScene::DrawDebugLines2D()
{
    if (!ctx_.debugRenderer) {
        return;
    }

    ctx_.debugRenderer->Render2D();
}
