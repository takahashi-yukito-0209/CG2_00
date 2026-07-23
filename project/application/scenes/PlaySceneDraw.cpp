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
constexpr uint32_t kGpuSpawnShapeSphere = 0; // GPU Emitterの球形状番号
constexpr uint32_t kGpuSpawnShapeBox = 1; // GPU Emitterの箱形状番号
constexpr uint32_t kGpuSpawnShapeRing = 2; // GPU Emitterのリング形状番号
constexpr uint32_t kGpuSpawnShapeCone = 3; // GPU Emitterのコーン形状番号
constexpr int kGpuEmitterCircleSegmentCount = 48; // GPU Emitter円形ワイヤーの分割数

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
    if (particleManager) {
        particleManager->Draw();
    }

    if (ctx_.debugRenderer) {
        ctx_.debugRenderer->DrawGrid(kDebugGridCenter, kDebugGridHalfLineCount, kDebugGridSpacing, kDebugGridColor);
        DrawEmitterDebugGrid(*ctx_.debugRenderer, pmEmitter_, kHitEmitterDebugRangeColor, kHitEmitterDebugGridColor);
        DrawEmitterDebugGrid(*ctx_.debugRenderer, ringEmitter_, kRingEmitterDebugRangeColor, kRingEmitterDebugGridColor);
        DrawEmitterDebugGrid(*ctx_.debugRenderer, cylinderEmitter_, kCylinderEmitterDebugRangeColor, kCylinderEmitterDebugGridColor);
        if (particleManager && particleManager->GetGpuEmitterState()) {
            DrawGpuEmitterDebugRange(*ctx_.debugRenderer, *particleManager->GetGpuEmitterState());
        }
    }
}

/// <summary>
/// ポストプロセスの影響を受けないスプライトを描画する。
/// </summary>
void PlayScene::DrawSprites()
{
    if (!ctx_.spriteCommon) {
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
