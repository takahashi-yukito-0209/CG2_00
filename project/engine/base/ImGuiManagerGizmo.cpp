#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "engine/2d/Sprite.h"
#include "engine/3d/Object3d.h"
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include "engine/utility/mathUtility.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace Math;

namespace MyEngine {
namespace {
constexpr float kGizmoAxisWorldLength = 1.5f; // 移動ギズモの軸を伸ばすワールド長
constexpr float kGizmoAxisLineThickness = 3.0f; // 移動ギズモ軸線の太さ
constexpr float kGizmoHandleRadius = 7.0f; // 移動ギズモのドラッグ判定半径
constexpr float kGizmoClickSelectRadius = 18.0f; // Scene Viewクリックでオブジェクトを選択できる半径
constexpr float kGizmoCenterHandleRadius = 10.0f; // 三軸移動ハンドルの判定半径
constexpr float kGizmoSceneViewLengthRate = 0.16f; // Scene Viewの短辺に対するギズモ軸長比率
constexpr float kGizmoTargetPixelMin = 70.0f; // ギズモ軸長の最小ピクセル
constexpr float kGizmoTargetPixelMax = 150.0f; // ギズモ軸長の最大ピクセル
constexpr float kGizmoModelScaleLengthRate = 1.25f; // モデルスケールから求める最低軸長倍率
constexpr float kGizmoMinimumPixelLength = 8.0f; // 操作可能とみなす軸の最小表示長
constexpr float kGizmoArrowLength = 16.0f; // 移動ギズモ矢印の長さ
constexpr float kGizmoArrowWidth = 9.0f; // 移動ギズモ矢印の幅
constexpr float kGizmoRotateSpeed = 0.01f; // 回転ギズモのドラッグ感度
constexpr float kGizmoRotateRingThickness = 2.0f; // 回転リングの太さ
constexpr int kGizmoRotateRingSegmentCount = 72; // 回転リングの分割数
constexpr float kGizmoRotateHitDistance = 12.0f; // 回転リングの選択距離
constexpr float kGizmoScaleSpeed = 0.01f; // 拡縮ギズモのドラッグ感度
constexpr float kGizmoScaleHandleHalfSize = 6.0f; // 拡縮ギズモ軸ハンドルの半分サイズ
constexpr float kGizmoMinimumScale = 0.001f; // ギズモ拡縮の最小スケール
constexpr float kSpriteGizmoRotateHandleOffset = 36.0f; // 2Dスプライト回転ハンドルをスプライト外側へ離す距離
constexpr float kSpriteGizmoCornerHandleSize = 8.0f; // 2Dスプライト拡縮ハンドルの半分サイズ
constexpr float kSpriteGizmoMinimumSize = 1.0f; // 2Dスプライトギズモで許可する最小サイズ

/// <summary>
/// 2Dベクトルを正規化して返す。
/// </summary>
ImVec2 NormalizeImVec2(const ImVec2& vector)
{
    const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y); // ベクトルの長さ
    if (length <= 0.000001f) {
        return { 0.0f, 0.0f };
    }
    return { vector.x / length, vector.y / length };
}

/// <summary>
/// 軸先端に矢印形状を描画する。
/// </summary>
void DrawGizmoArrowHead(ImDrawList* drawList, const ImVec2& origin, const ImVec2& axisEnd, ImU32 color)
{
    if (!drawList) {
        return;
    }

    const ImVec2 axisVector = { axisEnd.x - origin.x, axisEnd.y - origin.y }; // 画面上の軸方向
    const ImVec2 axisDirection = NormalizeImVec2(axisVector); // 正規化済みの軸方向
    if (axisDirection.x == 0.0f && axisDirection.y == 0.0f) {
        return;
    }

    const ImVec2 sideDirection = { -axisDirection.y, axisDirection.x }; // 矢印幅に使う垂直方向
    const ImVec2 arrowBase = {
        axisEnd.x - axisDirection.x * kGizmoArrowLength,
        axisEnd.y - axisDirection.y * kGizmoArrowLength
    }; // 矢印の根元
    const ImVec2 left = {
        arrowBase.x + sideDirection.x * kGizmoArrowWidth,
        arrowBase.y + sideDirection.y * kGizmoArrowWidth
    }; // 矢印左端
    const ImVec2 right = {
        arrowBase.x - sideDirection.x * kGizmoArrowWidth,
        arrowBase.y - sideDirection.y * kGizmoArrowWidth
    }; // 矢印右端
    drawList->AddTriangleFilled(axisEnd, left, right, color);
}

/// <summary>
/// 点と線分の距離を返す。
/// </summary>
float DistancePointToSegment(const ImVec2& point, const ImVec2& segmentStart, const ImVec2& segmentEnd)
{
    const ImVec2 segment = { segmentEnd.x - segmentStart.x, segmentEnd.y - segmentStart.y }; // 線分ベクトル
    const float segmentLengthSq = segment.x * segment.x + segment.y * segment.y; // 線分長の二乗
    if (segmentLengthSq <= 0.000001f) {
        const ImVec2 diff = { point.x - segmentStart.x, point.y - segmentStart.y }; // 始点から点への差分
        return std::sqrt(diff.x * diff.x + diff.y * diff.y);
    }

    const ImVec2 toPoint = { point.x - segmentStart.x, point.y - segmentStart.y }; // 始点から点へのベクトル
    const float t = (std::clamp)((toPoint.x * segment.x + toPoint.y * segment.y) / segmentLengthSq, 0.0f, 1.0f); // 最近点の線分上割合
    const ImVec2 closest = { segmentStart.x + segment.x * t, segmentStart.y + segment.y * t }; // 線分上の最近点
    const ImVec2 diff = { point.x - closest.x, point.y - closest.y }; // 最近点から点への差分
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
}

/// <summary>
/// Scene View表示サイズからスプライト座標の表示倍率を取得する。
/// </summary>
ImVec2 GetSceneViewSpriteScale(const ImGuiManager::Context& ctx, const ImVec2& imageSize)
{
    const float sourceWidth = ctx.sceneViewWidth > 0 ? static_cast<float>(ctx.sceneViewWidth) : imageSize.x; // 元のScene View幅
    const float sourceHeight = ctx.sceneViewHeight > 0 ? static_cast<float>(ctx.sceneViewHeight) : imageSize.y; // 元のScene View高さ
    return {
        sourceWidth > 0.0f ? imageSize.x / sourceWidth : 1.0f,
        sourceHeight > 0.0f ? imageSize.y / sourceHeight : 1.0f
    };
}

/// <summary>
/// スプライト座標をScene View上の座標へ変換する。
/// </summary>
ImVec2 SpritePositionToSceneView(const Vector2& position, const ImVec2& imageMin, const ImVec2& scale)
{
    return {
        imageMin.x + position.x * scale.x,
        imageMin.y + position.y * scale.y
    };
}

/// <summary>
/// Scene View上の移動量をスプライト座標の移動量へ変換する。
/// </summary>
Vector2 SceneViewDeltaToSpriteDelta(const ImVec2& delta, const ImVec2& scale)
{
    return {
        scale.x != 0.0f ? delta.x / scale.x : 0.0f,
        scale.y != 0.0f ? delta.y / scale.y : 0.0f
    };
}

/// <summary>
/// スプライトの四隅をScene View上の座標で計算する。
/// </summary>
std::array<ImVec2, 4> CalculateSpriteSceneViewCorners(Sprite& sprite, const ImVec2& imageMin, const ImVec2& scale)
{
    const Vector2 position = sprite.GetPosition(); // スプライトの基準座標
    const Vector2 size = sprite.GetSize(); // スプライトサイズ
    const Vector2 anchor = sprite.GetAnchorPoint(); // スプライトのアンカーポイント
    const float rotation = sprite.GetRotation(); // スプライトの回転角
    const float cosValue = std::cos(rotation); // 回転のcos値
    const float sinValue = std::sin(rotation); // 回転のsin値

    const Vector2 localCorners[] = {
        { -anchor.x * size.x, -anchor.y * size.y },
        { (1.0f - anchor.x) * size.x, -anchor.y * size.y },
        { (1.0f - anchor.x) * size.x, (1.0f - anchor.y) * size.y },
        { -anchor.x * size.x, (1.0f - anchor.y) * size.y },
    }; // アンカー基準のローカル四隅

    std::array<ImVec2, 4> corners {}; // Scene View上の四隅
    for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
        const Vector2 local = localCorners[cornerIndex]; // 計算対象のローカル座標
        const Vector2 rotated = {
            local.x * cosValue - local.y * sinValue,
            local.x * sinValue + local.y * cosValue
        }; // 回転後のローカル座標
        corners[cornerIndex] = SpritePositionToSceneView({ position.x + rotated.x, position.y + rotated.y }, imageMin, scale);
    }
    return corners;
}

/// <summary>
/// 点がスプライトの四隅で作る矩形内にあるかを判定する。
/// </summary>
bool IsPointInSpriteBounds(const ImVec2& point, const std::array<ImVec2, 4>& corners)
{
    bool hasPositive = false; // 正方向の外積があるか
    bool hasNegative = false; // 負方向の外積があるか
    for (int edgeIndex = 0; edgeIndex < 4; ++edgeIndex) {
        const ImVec2 start = corners[edgeIndex]; // 辺の始点
        const ImVec2 end = corners[(edgeIndex + 1) % 4]; // 辺の終点
        const float cross = (end.x - start.x) * (point.y - start.y) - (end.y - start.y) * (point.x - start.x); // 点と辺の外積
        hasPositive |= cross > 0.0f;
        hasNegative |= cross < 0.0f;
        if (hasPositive && hasNegative) {
            return false;
        }
    }
    return true;
}

/// <summary>
/// 回転行列で方向ベクトルだけを変換する。
/// </summary>
Vector3 TransformDirectionByMatrix(const Vector3& direction, const Matrix4x4& matrix)
{
    Vector3 result {}; // 変換後の方向
    result.x = direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0];
    result.y = direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1];
    result.z = direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2];
    return MathUtil::SafeNormalize(result, direction);
}
} // namespace

/// <summary>
/// ワールド座標をScene View上のスクリーン座標へ変換する
/// </summary>
bool ImGuiManager::ProjectWorldToSceneView(const Vector3& worldPosition, const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, ImVec2* outScreenPosition) const
{
    if (!outScreenPosition || imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
        return false;
    }

    const float clipX = worldPosition.x * viewProjectionMatrix.m[0][0] + worldPosition.y * viewProjectionMatrix.m[1][0] + worldPosition.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0];
    const float clipY = worldPosition.x * viewProjectionMatrix.m[0][1] + worldPosition.y * viewProjectionMatrix.m[1][1] + worldPosition.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1];
    const float clipZ = worldPosition.x * viewProjectionMatrix.m[0][2] + worldPosition.y * viewProjectionMatrix.m[1][2] + worldPosition.z * viewProjectionMatrix.m[2][2] + viewProjectionMatrix.m[3][2];
    const float clipW = worldPosition.x * viewProjectionMatrix.m[0][3] + worldPosition.y * viewProjectionMatrix.m[1][3] + worldPosition.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
    if (!std::isfinite(clipW) || std::fabs(clipW) <= 0.000001f) {
        return false;
    }

    const Vector3 ndcPosition = { clipX / clipW, clipY / clipW, clipZ / clipW };
    if (!std::isfinite(ndcPosition.x) || !std::isfinite(ndcPosition.y) || !std::isfinite(ndcPosition.z)) {
        return false;
    }

    outScreenPosition->x = imageMin.x + (ndcPosition.x + 1.0f) * 0.5f * imageSize.x;
    outScreenPosition->y = imageMin.y + (1.0f - (ndcPosition.y + 1.0f) * 0.5f) * imageSize.y;
    return ndcPosition.z >= 0.0f && ndcPosition.z <= 1.0f;
}

/// <summary>
/// Scene View上のクリック位置に近い3Dオブジェクトを選択する
/// </summary>
void ImGuiManager::SelectObjectBySceneViewClick(Context& ctx, const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (!ctx.objects3d || ctx.objects3d->empty() || !sceneViewHovered_) {
        return;
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    const ImVec2 mousePosition = ImGui::GetIO().MousePos; // Scene View上のクリック位置
    int nearestObjectIndex = -1; // クリック位置に最も近いオブジェクト番号
    float nearestDistanceSq = kGizmoClickSelectRadius * kGizmoClickSelectRadius; // 選択可能な最短距離の二乗

    const int objectCount = static_cast<int>(ctx.objects3d->size()); // 選択候補の3Dオブジェクト数
    for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
        Object3d* object = (*ctx.objects3d)[objectIndex]; // 選択候補の3Dオブジェクト
        if (!object) {
            continue;
        }

        ImVec2 objectScreenPosition {}; // オブジェクト原点のScene View上の位置
        if (!ProjectWorldToSceneView(object->GetTranslate(), viewProjectionMatrix, imageMin, imageSize, &objectScreenPosition)) {
            continue;
        }

        const float diffX = mousePosition.x - objectScreenPosition.x; // クリック位置との差分X
        const float diffY = mousePosition.y - objectScreenPosition.y; // クリック位置との差分Y
        const float distanceSq = diffX * diffX + diffY * diffY; // クリック位置との距離の二乗
        if (distanceSq <= nearestDistanceSq) {
            nearestDistanceSq = distanceSq;
            nearestObjectIndex = objectIndex;
        }
    }

    if (nearestObjectIndex >= 0) {
        selectedObjectIndex_ = nearestObjectIndex;
    }
}

/// <summary>
/// Scene View上のクリック位置に重なるスプライトを選択する。
/// </summary>
void ImGuiManager::SelectSpriteBySceneViewClick(Context& ctx, const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (!ctx.sprites || ctx.sprites->empty() || !sceneViewHovered_) {
        return;
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsAnyItemActive()) {
        return;
    }

    const ImVec2 mousePosition = ImGui::GetIO().MousePos; // Scene View上のクリック位置
    const ImVec2 spriteScale = GetSceneViewSpriteScale(ctx, imageSize); // スプライト座標からScene View座標への倍率
    const int spriteCount = static_cast<int>(ctx.sprites->size()); // 選択候補のスプライト数
    for (int spriteIndex = spriteCount - 1; spriteIndex >= 0; --spriteIndex) {
        Sprite* sprite = (*ctx.sprites)[spriteIndex]; // 選択候補のスプライト
        if (!sprite) {
            continue;
        }

        const std::array<ImVec2, 4> corners = CalculateSpriteSceneViewCorners(*sprite, imageMin, spriteScale); // Scene View上のスプライト四隅
        if (IsPointInSpriteBounds(mousePosition, corners)) {
            selectedSpriteIndex_ = spriteIndex;
            return;
        }
    }
}

/// <summary>
/// Scene View上に選択中スプライトの2Dギズモを描画する。
/// </summary>
void ImGuiManager::DrawSprite2DGizmo(Context& ctx, const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (!ctx.sprites || ctx.sprites->empty()) {
        return;
    }

    const int spriteCount = static_cast<int>(ctx.sprites->size()); // ギズモ対象候補のスプライト数
    selectedSpriteIndex_ = (std::clamp)(selectedSpriteIndex_, 0, spriteCount - 1);
    SelectSpriteBySceneViewClick(ctx, imageMin, imageSize);

    Sprite* selectedSprite = (*ctx.sprites)[selectedSpriteIndex_]; // ギズモ操作対象のスプライト
    if (!selectedSprite) {
        for (int spriteIndex = 0; spriteIndex < spriteCount; ++spriteIndex) {
            if ((*ctx.sprites)[spriteIndex]) {
                selectedSpriteIndex_ = spriteIndex;
                selectedSprite = (*ctx.sprites)[spriteIndex];
                break;
            }
        }
    }
    if (!selectedSprite) {
        return;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        activeGizmoOperationMode_ = -1;
        activeGizmoAxisIndex_ = -1;
    }

    const ImVec2 spriteScale = GetSceneViewSpriteScale(ctx, imageSize); // スプライト座標からScene View座標への倍率
    const Vector2 spritePosition = selectedSprite->GetPosition(); // スプライトの現在座標
    const std::array<ImVec2, 4> corners = CalculateSpriteSceneViewCorners(*selectedSprite, imageMin, spriteScale); // Scene View上の四隅
    const ImVec2 center = {
        (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * 0.25f,
        (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * 0.25f
    }; // Scene View上のスプライト表示中心
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int edgeIndex = 0; edgeIndex < 4; ++edgeIndex) {
        drawList->AddLine(corners[edgeIndex], corners[(edgeIndex + 1) % 4], IM_COL32(20, 20, 20, 180), 3.0f);
        drawList->AddLine(corners[edgeIndex], corners[(edgeIndex + 1) % 4], IM_COL32(250, 220, 80, 255), 1.5f);
    }

    if (gizmoOperationMode_ == 0) {
        const ImVec2 handleMin = { center.x - kGizmoCenterHandleRadius, center.y - kGizmoCenterHandleRadius }; // 中心ハンドル左上
        const ImVec2 handleMax = { center.x + kGizmoCenterHandleRadius, center.y + kGizmoCenterHandleRadius }; // 中心ハンドル右下
        drawList->AddCircleFilled(center, kGizmoCenterHandleRadius, IM_COL32(245, 245, 245, 230), 20);
        drawList->AddCircle(center, kGizmoCenterHandleRadius + 2.0f, IM_COL32(20, 20, 20, 180), 20, 2.0f);
        ImGui::SetCursorScreenPos(handleMin);
        ImGui::PushID("Sprite2DMoveHandle");
        ImGui::InvisibleButton("Sprite2DMoveHandle", { handleMax.x - handleMin.x, handleMax.y - handleMin.y });
        const bool isMoveActive = ImGui::IsItemActive(); // スプライト移動ハンドルをドラッグ中か
        const bool isMoveHovered = ImGui::IsItemHovered(); // スプライト移動ハンドル上にマウスがあるか
        const bool isMoveActivated = ImGui::IsItemActivated(); // スプライト移動ハンドルのドラッグ開始か
        ImGui::PopID();
        if (isMoveHovered || isMoveActive) {
            drawList->AddCircle(center, kGizmoCenterHandleRadius + 5.0f, IM_COL32(255, 255, 255, 230), 20, 2.0f);
        }
        if (isMoveActivated) {
            activeGizmoOperationMode_ = 0;
            activeGizmoAxisIndex_ = 0;
        }
        if (isMoveActive && activeGizmoOperationMode_ == 0) {
            const Vector2 spriteDelta = SceneViewDeltaToSpriteDelta(ImGui::GetIO().MouseDelta, spriteScale); // スプライト座標系の移動量
            selectedSprite->SetPosition({ spritePosition.x + spriteDelta.x, spritePosition.y + spriteDelta.y });
            selectedSprite->Update();
        }
        return;
    }

    if (gizmoOperationMode_ == 1) {
        const ImVec2 rotateHandle = center; // 回転操作を開始する画像中心位置
        drawList->AddCircleFilled(rotateHandle, kGizmoCenterHandleRadius, IM_COL32(80, 180, 255, 240), 20);
        drawList->AddCircle(rotateHandle, kGizmoCenterHandleRadius + 2.0f, IM_COL32(255, 255, 255, 230), 20, 2.0f);
        drawList->AddCircle(rotateHandle, kGizmoCenterHandleRadius + 6.0f, IM_COL32(20, 20, 20, 120), 24, 1.5f);

        ImGui::SetCursorScreenPos({ rotateHandle.x - kGizmoCenterHandleRadius, rotateHandle.y - kGizmoCenterHandleRadius });
        ImGui::PushID("Sprite2DRotateHandle");
        ImGui::InvisibleButton("Sprite2DRotateHandle", { kGizmoCenterHandleRadius * 2.0f, kGizmoCenterHandleRadius * 2.0f });
        const bool isRotateActive = ImGui::IsItemActive(); // スプライト回転ハンドルをドラッグ中か
        const bool isRotateActivated = ImGui::IsItemActivated(); // スプライト回転ハンドルのドラッグ開始か
        ImGui::PopID();
        if (isRotateActivated) {
            activeGizmoOperationMode_ = 1;
            activeGizmoAxisIndex_ = 0;
        }
        if (isRotateActive && activeGizmoOperationMode_ == 1) {
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            const float currentRotation = selectedSprite->GetRotation(); // 現在の回転角
            const float nextRotation = currentRotation + (mouseDelta.x - mouseDelta.y) * kGizmoRotateSpeed; // 操作後の回転角
            const Vector2 currentPosition = selectedSprite->GetPosition(); // 現在のアンカー位置
            const Vector2 currentSize = selectedSprite->GetSize(); // 現在のスプライトサイズ
            const Vector2 currentAnchor = selectedSprite->GetAnchorPoint(); // 現在のアンカーポイント
            const Vector2 centerLocalOffset = {
                (0.5f - currentAnchor.x) * currentSize.x,
                (0.5f - currentAnchor.y) * currentSize.y
            }; // アンカーから見た画像中心までのローカル差分
            const float currentCos = std::cos(currentRotation); // 現在回転のcos値
            const float currentSin = std::sin(currentRotation); // 現在回転のsin値
            const Vector2 currentCenterOffset = {
                centerLocalOffset.x * currentCos - centerLocalOffset.y * currentSin,
                centerLocalOffset.x * currentSin + centerLocalOffset.y * currentCos
            }; // 現在回転を反映した画像中心差分
            const Vector2 fixedCenterPosition = {
                currentPosition.x + currentCenterOffset.x,
                currentPosition.y + currentCenterOffset.y
            }; // 回転中に固定する画像中心位置
            const float nextCos = std::cos(nextRotation); // 操作後回転のcos値
            const float nextSin = std::sin(nextRotation); // 操作後回転のsin値
            const Vector2 nextCenterOffset = {
                centerLocalOffset.x * nextCos - centerLocalOffset.y * nextSin,
                centerLocalOffset.x * nextSin + centerLocalOffset.y * nextCos
            }; // 操作後回転を反映した画像中心差分
            selectedSprite->SetRotation(nextRotation);
            selectedSprite->SetPosition({ fixedCenterPosition.x - nextCenterOffset.x, fixedCenterPosition.y - nextCenterOffset.y });
            selectedSprite->Update();
        }
        return;
    }

    if (gizmoOperationMode_ == 2) {
        const ImVec2 rightDirection = NormalizeImVec2({ corners[1].x - corners[0].x, corners[1].y - corners[0].y }); // スプライト横方向
        const ImVec2 downDirection = NormalizeImVec2({ corners[3].x - corners[0].x, corners[3].y - corners[0].y }); // スプライト縦方向
        const int sizeSigns[4][2] = {
            { -1, -1 },
            { 1, -1 },
            { 1, 1 },
            { -1, 1 },
        }; // 各コーナーのサイズ増減方向

        for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
            const ImVec2 corner = corners[cornerIndex]; // 拡縮ハンドルの中心
            const ImVec2 min = { corner.x - kSpriteGizmoCornerHandleSize, corner.y - kSpriteGizmoCornerHandleSize }; // 拡縮ハンドル左上
            const ImVec2 max = { corner.x + kSpriteGizmoCornerHandleSize, corner.y + kSpriteGizmoCornerHandleSize }; // 拡縮ハンドル右下
            drawList->AddRectFilled(min, max, IM_COL32(80, 220, 230, 230), 1.0f);
            drawList->AddRect(min, max, IM_COL32(255, 255, 255, 230), 1.0f, 0, 1.5f);

            ImGui::SetCursorScreenPos(min);
            ImGui::PushID(cornerIndex);
            ImGui::InvisibleButton("Sprite2DScaleHandle", { max.x - min.x, max.y - min.y });
            const bool isScaleActive = ImGui::IsItemActive(); // スプライト拡縮ハンドルをドラッグ中か
            const bool isScaleHovered = ImGui::IsItemHovered(); // スプライト拡縮ハンドル上にマウスがあるか
            const bool isScaleActivated = ImGui::IsItemActivated(); // スプライト拡縮ハンドルのドラッグ開始か
            ImGui::PopID();
            if (isScaleHovered || isScaleActive) {
                drawList->AddRect({ min.x - 3.0f, min.y - 3.0f }, { max.x + 3.0f, max.y + 3.0f }, IM_COL32(255, 255, 255, 230), 1.0f, 0, 2.0f);
            }
            if (isScaleActivated) {
                activeGizmoOperationMode_ = 2;
                activeGizmoAxisIndex_ = cornerIndex;
            }
            if (isScaleActive && activeGizmoOperationMode_ == 2 && activeGizmoAxisIndex_ == cornerIndex) {
                const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
                const float widthDelta = (mouseDelta.x * rightDirection.x + mouseDelta.y * rightDirection.y) / (spriteScale.x != 0.0f ? spriteScale.x : 1.0f); // 横方向のサイズ変更量
                const float heightDelta = (mouseDelta.x * downDirection.x + mouseDelta.y * downDirection.y) / (spriteScale.y != 0.0f ? spriteScale.y : 1.0f); // 縦方向のサイズ変更量
                const Vector2 currentSize = selectedSprite->GetSize(); // 現在のスプライトサイズ
                selectedSprite->SetSize({
                    (std::max)(currentSize.x + widthDelta * static_cast<float>(sizeSigns[cornerIndex][0]), kSpriteGizmoMinimumSize),
                    (std::max)(currentSize.y + heightDelta * static_cast<float>(sizeSigns[cornerIndex][1]), kSpriteGizmoMinimumSize)
                });
                selectedSprite->Update();
            }
        }
        return;
    }
}

/// <summary>
/// Scene View上に選択中パーティクルエミッターの3Dギズモを描画する。
/// </summary>
void ImGuiManager::DrawParticleEmitterGizmo(Context& ctx, const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (!ctx.particleEmitters || ctx.particleEmitters->empty() || !ctx.sceneViewMatrix || !ctx.sceneProjectionMatrix) {
        return;
    }

    const int emitterCount = static_cast<int>(ctx.particleEmitters->size()); // ギズモ対象候補のエミッター数
    selectedEmitterIndex_ = (std::clamp)(selectedEmitterIndex_, 0, emitterCount - 1);

    if (sceneViewHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        const ImVec2 mousePosition = ImGui::GetIO().MousePos; // Scene View上のクリック位置
        int nearestEmitterIndex = -1; // クリック位置に最も近いエミッター番号
        float nearestDistanceSq = kGizmoClickSelectRadius * kGizmoClickSelectRadius; // 選択可能距離の二乗
        for (int emitterIndex = 0; emitterIndex < emitterCount; ++emitterIndex) {
            ParticleEmitter* emitter = (*ctx.particleEmitters)[emitterIndex]; // 選択候補のエミッター
            if (!emitter) {
                continue;
            }
            ImVec2 emitterScreenPosition {}; // エミッター原点のScene View上の位置
            if (!ProjectWorldToSceneView(emitter->transform.translate, viewProjectionMatrix, imageMin, imageSize, &emitterScreenPosition)) {
                continue;
            }
            const float diffX = mousePosition.x - emitterScreenPosition.x; // クリック位置との差分X
            const float diffY = mousePosition.y - emitterScreenPosition.y; // クリック位置との差分Y
            const float distanceSq = diffX * diffX + diffY * diffY; // クリック位置との距離の二乗
            if (distanceSq <= nearestDistanceSq) {
                nearestDistanceSq = distanceSq;
                nearestEmitterIndex = emitterIndex;
            }
        }
        if (nearestEmitterIndex >= 0) {
            selectedEmitterIndex_ = nearestEmitterIndex;
        }
    }

    ParticleEmitter* selectedEmitter = (*ctx.particleEmitters)[selectedEmitterIndex_]; // ギズモ操作対象のエミッター
    if (!selectedEmitter) {
        return;
    }

    const Vector3 originWorld = selectedEmitter->transform.translate; // ギズモ原点として使うエミッター位置
    ImVec2 originScreen {};
    if (!ProjectWorldToSceneView(originWorld, viewProjectionMatrix, imageMin, imageSize, &originScreen)) {
        return;
    }

    const float originClipW = originWorld.x * viewProjectionMatrix.m[0][3] + originWorld.y * viewProjectionMatrix.m[1][3] + originWorld.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
    const float sceneViewShortSide = (std::min)(imageSize.x, imageSize.y);
    const float targetPixelLength = (std::clamp)(sceneViewShortSide * kGizmoSceneViewLengthRate, kGizmoTargetPixelMin, kGizmoTargetPixelMax);
    const float pixelsPerWorld = imageSize.y * (*ctx.sceneProjectionMatrix).m[1][1] / ((std::max)(std::fabs(originClipW), 0.000001f) * 2.0f);
    const float axisWorldLength = pixelsPerWorld > 0.000001f ? targetPixelLength / pixelsPerWorld : kGizmoAxisWorldLength;

    const Vector3 globalAxisDirections[] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    }; // Global時のXYZ軸方向
    Vector3 axisDirections[] = {
        globalAxisDirections[0],
        globalAxisDirections[1],
        globalAxisDirections[2],
    }; // 現在の座標空間で使うXYZ軸方向
    if (gizmoTransformSpaceMode_ == 1) {
        const Matrix4x4 rotateMatrix = MathUtil::Multiply(
            MathUtil::MakeRotateXMatrix(selectedEmitter->transform.rotate.x),
            MathUtil::Multiply(MathUtil::MakeRotateYMatrix(selectedEmitter->transform.rotate.y), MathUtil::MakeRotateZMatrix(selectedEmitter->transform.rotate.z)));
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            axisDirections[axisIndex] = TransformDirectionByMatrix(globalAxisDirections[axisIndex], rotateMatrix);
        }
    }

    const ImU32 axisColors[] = {
        IM_COL32(230, 70, 70, 255),
        IM_COL32(80, 210, 100, 255),
        IM_COL32(80, 140, 240, 255),
    }; // XYZ軸の表示色
    const char* axisIds[] = { "EmitterX", "EmitterY", "EmitterZ" }; // エミッターギズモ用ID

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        activeGizmoOperationMode_ = -1;
        activeGizmoAxisIndex_ = -1;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircleFilled(originScreen, kGizmoCenterHandleRadius, IM_COL32(80, 230, 255, 230), 20);
    drawList->AddCircle(originScreen, kGizmoCenterHandleRadius + 2.0f, IM_COL32(20, 20, 20, 180), 20, 2.0f);

    if (gizmoOperationMode_ == 0) {
        Vector3 translate = selectedEmitter->transform.translate; // 操作後のエミッター位置
        bool changed = false; // 位置変更が発生したか
        ImGui::SetCursorScreenPos({ originScreen.x - kGizmoCenterHandleRadius, originScreen.y - kGizmoCenterHandleRadius });
        ImGui::PushID("EmitterMoveCenter");
        ImGui::InvisibleButton("EmitterMoveCenter", { kGizmoCenterHandleRadius * 2.0f, kGizmoCenterHandleRadius * 2.0f });
        const bool isCenterActive = ImGui::IsItemActive(); // 中心ハンドルをドラッグ中か
        const bool isCenterActivated = ImGui::IsItemActivated(); // 中心ハンドルのドラッグ開始か
        ImGui::PopID();
        if (isCenterActivated) {
            activeGizmoOperationMode_ = 0;
            activeGizmoAxisIndex_ = 3;
        }
        if (isCenterActive && activeGizmoOperationMode_ == 0 && activeGizmoAxisIndex_ == 3 && pixelsPerWorld > 0.000001f) {
            const Matrix4x4 inverseViewMatrix = MathUtil::Inverse(*ctx.sceneViewMatrix); // カメラ平面方向を得るための逆ビュー行列
            const Vector3 cameraRight = TransformDirectionByMatrix({ 1.0f, 0.0f, 0.0f }, inverseViewMatrix); // 画面右方向
            const Vector3 cameraUp = TransformDirectionByMatrix({ 0.0f, 1.0f, 0.0f }, inverseViewMatrix); // 画面上方向
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            translate += cameraRight * (mouseDelta.x / pixelsPerWorld);
            translate += cameraUp * (-mouseDelta.y / pixelsPerWorld);
            changed = true;
        }
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            ImVec2 axisEndScreen {};
            const Vector3 axisEndWorld = originWorld + axisDirections[axisIndex] * axisWorldLength;
            if (!ProjectWorldToSceneView(axisEndWorld, viewProjectionMatrix, imageMin, imageSize, &axisEndScreen)) {
                continue;
            }
            float moveAmount = 0.0f;
            bool isAxisActive = false;
            bool isAxisActivated = false;
            const bool movedAxis = DrawGizmoAxis(axisIds[axisIndex], originScreen, axisEndScreen, axisColors[axisIndex], axisWorldLength, true, &moveAmount, &isAxisActive, &isAxisActivated);
            if (isAxisActivated) {
                activeGizmoOperationMode_ = 0;
                activeGizmoAxisIndex_ = axisIndex;
            }
            if (movedAxis && activeGizmoOperationMode_ == 0 && activeGizmoAxisIndex_ == axisIndex) {
                translate += axisDirections[axisIndex] * moveAmount;
                changed = true;
            }
        }
        if (changed) {
            selectedEmitter->transform.translate = translate;
        }
        return;
    }

    if (gizmoOperationMode_ == 1) {
        ImGui::SetCursorScreenPos({ originScreen.x - kGizmoCenterHandleRadius, originScreen.y - kGizmoCenterHandleRadius });
        ImGui::PushID("EmitterRotateCenter");
        ImGui::InvisibleButton("EmitterRotateCenter", { kGizmoCenterHandleRadius * 2.0f, kGizmoCenterHandleRadius * 2.0f });
        const bool isRotateActive = ImGui::IsItemActive(); // 回転ハンドルをドラッグ中か
        ImGui::PopID();
        if (isRotateActive) {
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            selectedEmitter->transform.rotate.x += -mouseDelta.y * kGizmoRotateSpeed;
            selectedEmitter->transform.rotate.y += mouseDelta.x * kGizmoRotateSpeed;
            selectedEmitter->transform.rotate.z += (mouseDelta.x - mouseDelta.y) * kGizmoRotateSpeed * 0.5f;
        }
        return;
    }

    if (gizmoOperationMode_ == 2) {
        const float scaleHandleSize = (std::clamp)(targetPixelLength * 0.14f, 12.0f, 22.0f);
        const ImVec2 min = { originScreen.x - scaleHandleSize, originScreen.y - scaleHandleSize };
        const ImVec2 max = { originScreen.x + scaleHandleSize, originScreen.y + scaleHandleSize };
        drawList->AddRectFilled(min, max, IM_COL32(80, 220, 230, 230), 2.0f);
        drawList->AddRect(min, max, IM_COL32(255, 255, 255, 230), 2.0f, 0, 2.0f);
        ImGui::SetCursorScreenPos(min);
        ImGui::PushID("EmitterScaleCenter");
        ImGui::InvisibleButton("EmitterScaleCenter", { scaleHandleSize * 2.0f, scaleHandleSize * 2.0f });
        const bool isScaleActive = ImGui::IsItemActive(); // 拡縮ハンドルをドラッグ中か
        ImGui::PopID();
        if (isScaleActive) {
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            const float scaleRate = (std::max)(1.0f + (mouseDelta.x - mouseDelta.y) * kGizmoScaleSpeed, 0.01f);
            selectedEmitter->transform.scale.x = (std::max)(selectedEmitter->transform.scale.x * scaleRate, kGizmoMinimumScale);
            selectedEmitter->transform.scale.y = (std::max)(selectedEmitter->transform.scale.y * scaleRate, kGizmoMinimumScale);
            selectedEmitter->transform.scale.z = (std::max)(selectedEmitter->transform.scale.z * scaleRate, kGizmoMinimumScale);
        }
        return;
    }
}

/// <summary>
/// Scene View上にGPU Emitterの3Dギズモを描画する。
/// </summary>
void ImGuiManager::DrawGpuParticleEmitterGizmo(Context& ctx, const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (!ctx.particleManager || !ctx.sceneViewMatrix || !ctx.sceneProjectionMatrix) {
        return;
    }

    PM_GpuEmitterSphere* gpuEmitter = ctx.particleManager->GetMutableGpuEmitterState(); // 操作対象のGPU Emitter設定
    if (!gpuEmitter) {
        return;
    }

    const Vector3 originWorld = gpuEmitter->translate; // ギズモ原点として使うGPU Emitter位置
    ImVec2 originScreen {};
    if (!ProjectWorldToSceneView(originWorld, viewProjectionMatrix, imageMin, imageSize, &originScreen)) {
        return;
    }

    const float originClipW = originWorld.x * viewProjectionMatrix.m[0][3] + originWorld.y * viewProjectionMatrix.m[1][3] + originWorld.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3]; // ギズモ距離の計算に使うクリップW
    const float sceneViewShortSide = (std::min)(imageSize.x, imageSize.y); // Scene Viewの短辺
    const float targetPixelLength = (std::clamp)(sceneViewShortSide * kGizmoSceneViewLengthRate, kGizmoTargetPixelMin, kGizmoTargetPixelMax); // 表示上のギズモ軸長
    const float pixelsPerWorld = imageSize.y * (*ctx.sceneProjectionMatrix).m[1][1] / ((std::max)(std::fabs(originClipW), 0.000001f) * 2.0f); // 1ワールド単位の画面ピクセル数
    const float axisWorldLength = pixelsPerWorld > 0.000001f ? targetPixelLength / pixelsPerWorld : kGizmoAxisWorldLength; // ワールド上のギズモ軸長

    const Vector3 axisDirections[] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    }; // GPU Emitterで使うグローバルXYZ軸
    const ImU32 axisColors[] = {
        IM_COL32(230, 70, 70, 255),
        IM_COL32(80, 210, 100, 255),
        IM_COL32(80, 140, 240, 255),
    }; // XYZ軸の表示色
    const char* axisIds[] = { "GpuEmitterX", "GpuEmitterY", "GpuEmitterZ" }; // GPU Emitterギズモ用ID

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        activeGizmoOperationMode_ = -1;
        activeGizmoAxisIndex_ = -1;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList(); // Scene View上に重ねる描画リスト
    drawList->AddCircleFilled(originScreen, kGizmoCenterHandleRadius, IM_COL32(180, 100, 255, 235), 20);
    drawList->AddCircle(originScreen, kGizmoCenterHandleRadius + 2.0f, IM_COL32(20, 20, 20, 180), 20, 2.0f);

    if (gizmoOperationMode_ == 0) {
        Vector3 translate = gpuEmitter->translate; // 操作後のGPU Emitter位置
        bool changed = false; // 位置変更が発生したか
        const ImVec2 centerMin = { originScreen.x - kGizmoCenterHandleRadius, originScreen.y - kGizmoCenterHandleRadius }; // 中心ハンドル左上
        const ImVec2 centerMax = { originScreen.x + kGizmoCenterHandleRadius, originScreen.y + kGizmoCenterHandleRadius }; // 中心ハンドル右下
        ImGui::SetCursorScreenPos(centerMin);
        ImGui::PushID("GpuEmitterMoveCenter");
        ImGui::InvisibleButton("GpuEmitterMoveCenter", { centerMax.x - centerMin.x, centerMax.y - centerMin.y });
        const bool isCenterActive = ImGui::IsItemActive(); // 中心ハンドルをドラッグ中か
        const bool isCenterActivated = ImGui::IsItemActivated(); // 中心ハンドルのドラッグ開始か
        ImGui::PopID();
        if (isCenterActivated) {
            activeGizmoOperationMode_ = 0;
            activeGizmoAxisIndex_ = 3;
        }
        if (isCenterActive && activeGizmoOperationMode_ == 0 && activeGizmoAxisIndex_ == 3 && pixelsPerWorld > 0.000001f) {
            const Matrix4x4 inverseViewMatrix = MathUtil::Inverse(*ctx.sceneViewMatrix); // カメラ平面方向を得るための逆ビュー行列
            const Vector3 cameraRight = TransformDirectionByMatrix({ 1.0f, 0.0f, 0.0f }, inverseViewMatrix); // 画面右方向
            const Vector3 cameraUp = TransformDirectionByMatrix({ 0.0f, 1.0f, 0.0f }, inverseViewMatrix); // 画面上方向
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            translate += cameraRight * (mouseDelta.x / pixelsPerWorld);
            translate += cameraUp * (-mouseDelta.y / pixelsPerWorld);
            changed = true;
        }
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            ImVec2 axisEndScreen {}; // 軸端のScene View上の位置
            const Vector3 axisEndWorld = originWorld + axisDirections[axisIndex] * axisWorldLength; // 軸端のワールド位置
            if (!ProjectWorldToSceneView(axisEndWorld, viewProjectionMatrix, imageMin, imageSize, &axisEndScreen)) {
                continue;
            }
            float moveAmount = 0.0f; // 軸方向の移動量
            bool isAxisActive = false; // 軸ハンドルをドラッグ中か
            bool isAxisActivated = false; // 軸ハンドルのドラッグ開始か
            const bool movedAxis = DrawGizmoAxis(axisIds[axisIndex], originScreen, axisEndScreen, axisColors[axisIndex], axisWorldLength, true, &moveAmount, &isAxisActive, &isAxisActivated);
            if (isAxisActivated) {
                activeGizmoOperationMode_ = 0;
                activeGizmoAxisIndex_ = axisIndex;
            }
            if (movedAxis && activeGizmoOperationMode_ == 0 && activeGizmoAxisIndex_ == axisIndex) {
                translate += axisDirections[axisIndex] * moveAmount;
                changed = true;
            }
        }
        if (changed) {
            gpuEmitter->translate = translate;
        }
        return;
    }

    if (gizmoOperationMode_ == 2) {
        const float scaleHandleSize = (std::clamp)(targetPixelLength * 0.14f, 12.0f, 22.0f); // 半径操作ハンドルの表示サイズ
        const ImVec2 min = { originScreen.x - scaleHandleSize, originScreen.y - scaleHandleSize }; // ハンドル左上
        const ImVec2 max = { originScreen.x + scaleHandleSize, originScreen.y + scaleHandleSize }; // ハンドル右下
        drawList->AddRectFilled(min, max, IM_COL32(180, 100, 255, 235), 2.0f);
        drawList->AddRect(min, max, IM_COL32(255, 255, 255, 230), 2.0f, 0, 2.0f);
        ImGui::SetCursorScreenPos(min);
        ImGui::PushID("GpuEmitterScaleRadius");
        ImGui::InvisibleButton("GpuEmitterScaleRadius", { scaleHandleSize * 2.0f, scaleHandleSize * 2.0f });
        const bool isScaleActive = ImGui::IsItemActive(); // 半径ハンドルをドラッグ中か
        ImGui::PopID();
        if (isScaleActive) {
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            const float scaleRate = (std::max)(1.0f + (mouseDelta.x - mouseDelta.y) * kGizmoScaleSpeed, 0.01f); // 半径倍率
            gpuEmitter->radius = (std::max)(gpuEmitter->radius * scaleRate, kGizmoMinimumScale);
        }
        return;
    }
}

/// <summary>
/// 現在の描画対象に対応するギズモ操作対象番号を取得する
/// </summary>
int ImGuiManager::ResolveGizmoObjectIndex(const Context& ctx) const
{
    (void)ctx;
    return selectedObjectIndex_;
}

/// <summary>
/// 移動ギズモの1軸を描画し、ドラッグ時は平行移動量を返す
/// </summary>
bool ImGuiManager::DrawGizmoAxis(const char* id, const ImVec2& origin, const ImVec2& axisEnd, ImU32 color, float axisWorldLength, bool drawAxis, float* outMoveAmount, bool* outIsActive, bool* outIsActivated)
{
    if (!id || !outMoveAmount || !outIsActive || !outIsActivated) {
        return false;
    }

    *outMoveAmount = 0.0f;
    *outIsActive = false;
    *outIsActivated = false;

    const ImVec2 axisVector = { axisEnd.x - origin.x, axisEnd.y - origin.y }; // 画面上の軸方向
    const float axisLength = std::sqrt(axisVector.x * axisVector.x + axisVector.y * axisVector.y); // 画面上の軸長
    if (axisLength < kGizmoMinimumPixelLength) {
        return false;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (drawAxis) {
        drawList->AddLine(origin, axisEnd, IM_COL32(20, 20, 20, 180), kGizmoAxisLineThickness + 2.0f);
        drawList->AddLine(origin, axisEnd, color, kGizmoAxisLineThickness);
        DrawGizmoArrowHead(drawList, origin, axisEnd, color);
    }

    const float hitPadding = kGizmoHandleRadius + 3.0f; // 軸線のドラッグ判定幅
    const ImVec2 hitMin = {
        (std::min)(origin.x, axisEnd.x) - hitPadding,
        (std::min)(origin.y, axisEnd.y) - hitPadding
    }; // 軸判定矩形の左上
    const ImVec2 hitMax = {
        (std::max)(origin.x, axisEnd.x) + hitPadding,
        (std::max)(origin.y, axisEnd.y) + hitPadding
    }; // 軸判定矩形の右下
    ImGui::SetCursorScreenPos(hitMin);
    ImGui::PushID(id);
    ImGui::InvisibleButton("AxisHandle", { hitMax.x - hitMin.x, hitMax.y - hitMin.y });
    const ImVec2 mousePosition = ImGui::GetIO().MousePos; // 現在のマウス位置
    const bool isNearAxis = DistancePointToSegment(mousePosition, origin, axisEnd) <= hitPadding; // 軸線の近くにマウスがあるか
    const bool isActive = ImGui::IsItemActive(); // この軸をドラッグ中か
    const bool isHovered = ImGui::IsItemHovered() && isNearAxis; // この軸にマウスが乗っているか
    const bool isActivated = ImGui::IsItemActivated() && isNearAxis; // この軸のドラッグを開始したか
    ImGui::PopID();

    *outIsActive = isActive;
    *outIsActivated = isActivated;

    if (drawAxis && (isHovered || isActive)) {
        drawList->AddCircle(axisEnd, kGizmoHandleRadius + 5.0f, IM_COL32(255, 255, 255, 220), 16, 2.0f);
        drawList->AddLine(origin, axisEnd, IM_COL32(255, 255, 255, 150), kGizmoAxisLineThickness + 1.0f);
    }
    if (!isActive) {
        return false;
    }

    const ImVec2 axisDirection = { axisVector.x / axisLength, axisVector.y / axisLength }; // 正規化済みの軸方向
    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
    *outMoveAmount = (mouseDelta.x * axisDirection.x + mouseDelta.y * axisDirection.y) / axisLength * axisWorldLength;
    return std::fabs(*outMoveAmount) > 0.0f;
}

/// <summary>
/// Scene View上に選択中オブジェクトの移動ギズモを描画する
/// </summary>
void ImGuiManager::DrawTranslationGizmo(Context& ctx, const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (gizmoTargetMode_ == 1) {
        DrawSprite2DGizmo(ctx, imageMin, imageSize);
        return;
    }

    if (!ctx.sceneViewMatrix || !ctx.sceneProjectionMatrix) {
        return;
    }

    const Matrix4x4 viewProjectionMatrix = MathUtil::Multiply(*ctx.sceneViewMatrix, *ctx.sceneProjectionMatrix); // Scene View用のビュー射影行列
    if (gizmoTargetMode_ == 2) {
        DrawParticleEmitterGizmo(ctx, viewProjectionMatrix, imageMin, imageSize);
        return;
    }
    if (gizmoTargetMode_ == 3) {
        DrawGpuParticleEmitterGizmo(ctx, viewProjectionMatrix, imageMin, imageSize);
        return;
    }
    if (gizmoTargetMode_ == 4) {
        return; // Level ColliderはScene固有Overlay側で描画と編集を行う
    }

    if (!ctx.objects3d || ctx.objects3d->empty()) {
        return;
    }
    const int objectCount = static_cast<int>(ctx.objects3d->size()); // ギズモ対象候補の3Dオブジェクト数
    SelectObjectBySceneViewClick(ctx, viewProjectionMatrix, imageMin, imageSize);

    selectedObjectIndex_ = (std::clamp)(selectedObjectIndex_, 0, objectCount - 1);
    int resolvedObjectIndex = ResolveGizmoObjectIndex(ctx); // 実際にギズモを表示するオブジェクト番号
    if (resolvedObjectIndex < 0 || resolvedObjectIndex >= objectCount) {
        return;
    }
    if (!(*ctx.objects3d)[resolvedObjectIndex]) {
        for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
            if ((*ctx.objects3d)[objectIndex]) {
                selectedObjectIndex_ = objectIndex;
                resolvedObjectIndex = objectIndex;
                break;
            }
        }
    }
    if (!(*ctx.objects3d)[resolvedObjectIndex]) {
        return;
    }

    Object3d* selectedObject = (*ctx.objects3d)[resolvedObjectIndex]; // ギズモ操作対象の3Dオブジェクト
    if (!selectedObject) {
        return;
    }

    const Vector3 originWorld = selectedObject->GetTranslate(); // ギズモ原点として使うオブジェクト位置
    ImVec2 originScreen {};
    if (!ProjectWorldToSceneView(originWorld, viewProjectionMatrix, imageMin, imageSize, &originScreen)) {
        return;
    }

    const float originClipW = originWorld.x * viewProjectionMatrix.m[0][3] + originWorld.y * viewProjectionMatrix.m[1][3] + originWorld.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
    const float sceneViewShortSide = (std::min)(imageSize.x, imageSize.y);
    const float targetPixelLength = (std::clamp)(sceneViewShortSide * kGizmoSceneViewLengthRate, kGizmoTargetPixelMin, kGizmoTargetPixelMax);
    const float pixelsPerWorld = imageSize.y * (*ctx.sceneProjectionMatrix).m[1][1] / ((std::max)(std::fabs(originClipW), 0.000001f) * 2.0f);
    const float depthBasedAxisLength = pixelsPerWorld > 0.000001f ? targetPixelLength / pixelsPerWorld : kGizmoAxisWorldLength;
    const float axisWorldLength = depthBasedAxisLength;

    const Vector3 globalAxisDirections[] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    }; // Global時のXYZ軸方向
    Vector3 axisDirections[] = {
        globalAxisDirections[0],
        globalAxisDirections[1],
        globalAxisDirections[2],
    }; // 現在の座標空間で使うXYZ軸方向
    if (gizmoTransformSpaceMode_ == 1) {
        const Vector3 objectRotate = selectedObject->GetRotate(); // 選択オブジェクトの現在回転
        const Matrix4x4 rotateMatrix = MathUtil::Multiply(
            MathUtil::MakeRotateXMatrix(objectRotate.x),
            MathUtil::Multiply(MathUtil::MakeRotateYMatrix(objectRotate.y), MathUtil::MakeRotateZMatrix(objectRotate.z)));
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            axisDirections[axisIndex] = TransformDirectionByMatrix(globalAxisDirections[axisIndex], rotateMatrix);
        }
    }
    const Vector3 axisWorlds[] = {
        axisDirections[0] * axisWorldLength,
        axisDirections[1] * axisWorldLength,
        axisDirections[2] * axisWorldLength,
    }; // 現在の座標空間を反映した軸ベクトル
    const ImU32 axisColors[] = {
        IM_COL32(230, 70, 70, 255),
        IM_COL32(80, 210, 100, 255),
        IM_COL32(80, 140, 240, 255),
    };
    const char* axisIds[] = { "X", "Y", "Z" };

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        activeGizmoOperationMode_ = -1;
        activeGizmoAxisIndex_ = -1;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (gizmoOperationMode_ == 0) {
        Vector3 translate = selectedObject->GetTranslate();
        bool changed = false;

        const ImVec2 centerMin = { originScreen.x - kGizmoCenterHandleRadius, originScreen.y - kGizmoCenterHandleRadius };
        const ImVec2 centerMax = { originScreen.x + kGizmoCenterHandleRadius, originScreen.y + kGizmoCenterHandleRadius };
        drawList->AddCircleFilled(originScreen, kGizmoCenterHandleRadius, IM_COL32(245, 245, 245, 230), 20);
        drawList->AddCircle(originScreen, kGizmoCenterHandleRadius + 2.0f, IM_COL32(20, 20, 20, 180), 20, 2.0f);
        ImGui::SetCursorScreenPos(centerMin);
        ImGui::PushID("MoveAllAxesHandle");
        ImGui::InvisibleButton("MoveAllAxesHandle", { centerMax.x - centerMin.x, centerMax.y - centerMin.y });
        const bool isCenterActive = ImGui::IsItemActive(); // 三軸移動ハンドルをドラッグ中か
        const bool isCenterHovered = ImGui::IsItemHovered(); // 三軸移動ハンドル上にマウスがあるか
        const bool isCenterActivated = ImGui::IsItemActivated(); // 三軸移動ハンドルのドラッグ開始か
        ImGui::PopID();
        if (isCenterHovered || isCenterActive) {
            drawList->AddCircle(originScreen, kGizmoCenterHandleRadius + 5.0f, IM_COL32(255, 255, 255, 230), 20, 2.0f);
        }
        if (isCenterActivated) {
            activeGizmoOperationMode_ = 0;
            activeGizmoAxisIndex_ = 3;
        }
        if (isCenterActive && activeGizmoOperationMode_ == 0 && activeGizmoAxisIndex_ == 3 && pixelsPerWorld > 0.000001f) {
            const Matrix4x4 inverseViewMatrix = MathUtil::Inverse(*ctx.sceneViewMatrix); // カメラ平面方向を得るための逆ビュー行列
            const Vector3 cameraRight = TransformDirectionByMatrix({ 1.0f, 0.0f, 0.0f }, inverseViewMatrix); // 画面右方向
            const Vector3 cameraUp = TransformDirectionByMatrix({ 0.0f, 1.0f, 0.0f }, inverseViewMatrix); // 画面上方向
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            translate += cameraRight * (mouseDelta.x / pixelsPerWorld);
            translate += cameraUp * (-mouseDelta.y / pixelsPerWorld);
            changed = true;
        }

        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            ImVec2 axisEndScreen {};
            const Vector3 axisEndWorld = originWorld + axisWorlds[axisIndex];
            if (!ProjectWorldToSceneView(axisEndWorld, viewProjectionMatrix, imageMin, imageSize, &axisEndScreen)) {
                continue;
            }

            const bool drawAxis = activeGizmoOperationMode_ != 0 || activeGizmoAxisIndex_ < 0 || activeGizmoAxisIndex_ == axisIndex || activeGizmoAxisIndex_ == 3; // 操作中は対象軸だけ表示
            float moveAmount = 0.0f;
            bool isAxisActive = false;
            bool isAxisActivated = false;
            const bool movedAxis = DrawGizmoAxis(axisIds[axisIndex], originScreen, axisEndScreen, axisColors[axisIndex], axisWorldLength, drawAxis, &moveAmount, &isAxisActive, &isAxisActivated);
            if (isAxisActivated) {
                activeGizmoOperationMode_ = 0;
                activeGizmoAxisIndex_ = axisIndex;
            }
            if (movedAxis && activeGizmoOperationMode_ == 0 && activeGizmoAxisIndex_ == axisIndex) {
                translate += axisDirections[axisIndex] * moveAmount;
                changed = true;
            }
        }

        if (changed) {
            selectedObject->SetTranslate(translate);
            if (ctx.notifyObjectTransformEdited) {
                ctx.notifyObjectTransformEdited(static_cast<size_t>(resolvedObjectIndex));
            }
        }
        return;
    }

    if (gizmoOperationMode_ == 1) {
        const float ringWorldRadius = axisWorldLength * 0.72f; // 3D空間上の回転リング半径
        std::array<std::array<ImVec2, kGizmoRotateRingSegmentCount + 1>, 3> ringPoints {}; // 各軸リングの投影点
        std::array<std::array<bool, kGizmoRotateRingSegmentCount + 1>, 3> ringVisible {}; // 各投影点が表示可能か
        std::array<float, 3> closestDistances = { 999999.0f, 999999.0f, 999999.0f }; // マウスから各リングへの最短距離
        const ImVec2 mousePosition = ImGui::GetIO().MousePos; // 現在のマウス位置

        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            for (int segmentIndex = 0; segmentIndex <= kGizmoRotateRingSegmentCount; ++segmentIndex) {
                const float rate = static_cast<float>(segmentIndex) / static_cast<float>(kGizmoRotateRingSegmentCount); // リング上の割合
                const float angle = rate * 6.28318530718f; // リング上の角度
                const float c = std::cos(angle); // 円周上X成分
                const float s = std::sin(angle); // 円周上Y成分
                Vector3 ringWorld = originWorld; // リング上のワールド座標
                if (axisIndex == 0) {
                    ringWorld += axisDirections[1] * (c * ringWorldRadius) + axisDirections[2] * (s * ringWorldRadius);
                } else if (axisIndex == 1) {
                    ringWorld += axisDirections[0] * (c * ringWorldRadius) + axisDirections[2] * (s * ringWorldRadius);
                } else {
                    ringWorld += axisDirections[0] * (c * ringWorldRadius) + axisDirections[1] * (s * ringWorldRadius);
                }
                ringVisible[axisIndex][segmentIndex] = ProjectWorldToSceneView(ringWorld, viewProjectionMatrix, imageMin, imageSize, &ringPoints[axisIndex][segmentIndex]);
            }
        }

        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            for (int segmentIndex = 0; segmentIndex < kGizmoRotateRingSegmentCount; ++segmentIndex) {
                if (!ringVisible[axisIndex][segmentIndex] || !ringVisible[axisIndex][segmentIndex + 1]) {
                    continue;
                }
                closestDistances[axisIndex] = (std::min)(closestDistances[axisIndex], DistancePointToSegment(mousePosition, ringPoints[axisIndex][segmentIndex], ringPoints[axisIndex][segmentIndex + 1]));
            }
        }

        int hoveredRotateAxis = -1; // マウスに最も近い回転軸
        float hoveredDistance = kGizmoRotateHitDistance; // 選択可能な距離
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            if (closestDistances[axisIndex] < hoveredDistance) {
                hoveredDistance = closestDistances[axisIndex];
                hoveredRotateAxis = axisIndex;
            }
        }

        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            if (activeGizmoOperationMode_ == 1 && activeGizmoAxisIndex_ >= 0 && activeGizmoAxisIndex_ != axisIndex) {
                continue;
            }
            const bool isHoveredAxis = hoveredRotateAxis == axisIndex; // 強調表示する軸か
            const bool isActiveAxis = activeGizmoOperationMode_ == 1 && activeGizmoAxisIndex_ == axisIndex; // 操作中の軸か
            const float thickness = (isHoveredAxis || isActiveAxis) ? kGizmoRotateRingThickness + 2.0f : kGizmoRotateRingThickness;
            for (int segmentIndex = 0; segmentIndex < kGizmoRotateRingSegmentCount; ++segmentIndex) {
                if (!ringVisible[axisIndex][segmentIndex] || !ringVisible[axisIndex][segmentIndex + 1]) {
                    continue;
                }
                drawList->AddLine(ringPoints[axisIndex][segmentIndex], ringPoints[axisIndex][segmentIndex + 1], IM_COL32(20, 20, 20, 170), thickness + 2.0f);
                drawList->AddLine(ringPoints[axisIndex][segmentIndex], ringPoints[axisIndex][segmentIndex + 1], axisColors[axisIndex], thickness);
            }
        }

        const float rotateHitRadius = targetPixelLength * 0.70f; // 回転ギズモ全体の入力領域半径
        ImGui::SetCursorScreenPos({ originScreen.x - rotateHitRadius, originScreen.y - rotateHitRadius });
        ImGui::PushID("RotateHandle");
        ImGui::InvisibleButton("RotateHandle", { rotateHitRadius * 2.0f, rotateHitRadius * 2.0f });
        const bool isRotateActive = ImGui::IsItemActive(); // 回転リングをドラッグ中か
        const bool isRotateHovered = ImGui::IsItemHovered(); // 回転リング入力領域上か
        const bool isRotateStarted = ImGui::IsItemActivated(); // 回転ドラッグを開始した瞬間か
        ImGui::PopID();

        if (isRotateStarted && hoveredRotateAxis >= 0) {
            activeGizmoOperationMode_ = 1;
            activeGizmoAxisIndex_ = hoveredRotateAxis;
        }

        const int operationRotateAxis = isRotateActive ? activeGizmoAxisIndex_ : hoveredRotateAxis; // 操作対象の回転軸
        if ((isRotateHovered || isRotateActive) && operationRotateAxis >= 0) {
            Vector3 rotate = selectedObject->GetRotate(); // 選択オブジェクトの現在回転
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            const float rotateAmount = (mouseDelta.x - mouseDelta.y) * kGizmoRotateSpeed; // ドラッグから求めた回転量
            if (isRotateActive) {
                if (operationRotateAxis == 0) {
                    rotate.x += rotateAmount;
                } else if (operationRotateAxis == 1) {
                    rotate.y += rotateAmount;
                } else {
                    rotate.z += rotateAmount;
                }
                selectedObject->SetRotate(rotate);
                if (ctx.notifyObjectTransformEdited) {
                    ctx.notifyObjectTransformEdited(static_cast<size_t>(resolvedObjectIndex));
                }
            }
        }
        return;
    }
    if (gizmoOperationMode_ == 2) {
        Vector3 scale = selectedObject->GetScale(); // 選択オブジェクトの現在スケール
        bool changed = false; // スケール変更が発生したか
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            ImVec2 axisEndScreen {};
            const Vector3 axisEndWorld = originWorld + axisWorlds[axisIndex]; // 軸ハンドルのワールド位置
            if (!ProjectWorldToSceneView(axisEndWorld, viewProjectionMatrix, imageMin, imageSize, &axisEndScreen)) {
                continue;
            }
            if (activeGizmoOperationMode_ == 2 && activeGizmoAxisIndex_ >= 0 && activeGizmoAxisIndex_ != axisIndex) {
                continue;
            }

            drawList->AddLine(originScreen, axisEndScreen, IM_COL32(20, 20, 20, 170), kGizmoAxisLineThickness + 2.0f);
            drawList->AddLine(originScreen, axisEndScreen, axisColors[axisIndex], kGizmoAxisLineThickness);
            const ImVec2 min = { axisEndScreen.x - kGizmoScaleHandleHalfSize, axisEndScreen.y - kGizmoScaleHandleHalfSize };
            const ImVec2 max = { axisEndScreen.x + kGizmoScaleHandleHalfSize, axisEndScreen.y + kGizmoScaleHandleHalfSize };
            drawList->AddRectFilled(min, max, axisColors[axisIndex], 1.0f);
            drawList->AddRect(min, max, IM_COL32(255, 255, 255, 230), 1.0f, 0, 1.5f);

            const ImVec2 axisVector = { axisEndScreen.x - originScreen.x, axisEndScreen.y - originScreen.y }; // 画面上の軸方向
            const float axisLength = std::sqrt(axisVector.x * axisVector.x + axisVector.y * axisVector.y); // 画面上の軸長
            if (axisLength < kGizmoMinimumPixelLength) {
                continue;
            }
            const ImVec2 axisDirection = { axisVector.x / axisLength, axisVector.y / axisLength }; // 正規化済みの軸方向
            ImGui::SetCursorScreenPos({ axisEndScreen.x - kGizmoHandleRadius, axisEndScreen.y - kGizmoHandleRadius });
            ImGui::PushID(axisIds[axisIndex]);
            ImGui::InvisibleButton("ScaleAxisHandle", { kGizmoHandleRadius * 2.0f, kGizmoHandleRadius * 2.0f });
            const bool isAxisActive = ImGui::IsItemActive(); // 軸別拡縮ハンドルをドラッグ中か
            const bool isAxisHovered = ImGui::IsItemHovered(); // 軸別拡縮ハンドル上にマウスがあるか
            const bool isAxisActivated = ImGui::IsItemActivated(); // 軸別拡縮ハンドルのドラッグを開始したか
            ImGui::PopID();
            if (isAxisActivated) {
                activeGizmoOperationMode_ = 2;
                activeGizmoAxisIndex_ = axisIndex;
            }
            if (isAxisHovered || isAxisActive) {
                drawList->AddRect({ min.x - 3.0f, min.y - 3.0f }, { max.x + 3.0f, max.y + 3.0f }, IM_COL32(255, 255, 255, 230), 1.0f, 0, 2.0f);
            }
            if (isAxisActive) {
                const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
                const float scaleAmount = (mouseDelta.x * axisDirection.x + mouseDelta.y * axisDirection.y) / axisLength * axisWorldLength; // 軸方向の拡縮量
                if (axisIndex == 0) {
                    scale.x = (std::max)(scale.x + scaleAmount, kGizmoMinimumScale);
                } else if (axisIndex == 1) {
                    scale.y = (std::max)(scale.y + scaleAmount, kGizmoMinimumScale);
                } else {
                    scale.z = (std::max)(scale.z + scaleAmount, kGizmoMinimumScale);
                }
                changed = true;
            }
        }

        const float scaleHandleSize = (std::clamp)(targetPixelLength * 0.14f, 12.0f, 22.0f);
        const ImVec2 centerMin = { originScreen.x - scaleHandleSize, originScreen.y - scaleHandleSize };
        const ImVec2 centerMax = { originScreen.x + scaleHandleSize, originScreen.y + scaleHandleSize };
        drawList->AddRectFilled(centerMin, centerMax, IM_COL32(80, 220, 230, 230), 2.0f);
        drawList->AddRect(centerMin, centerMax, IM_COL32(255, 255, 255, 230), 2.0f, 0, 2.0f);
        ImGui::SetCursorScreenPos(centerMin);
        ImGui::PushID("ScaleUniformHandle");
        ImGui::InvisibleButton("ScaleUniformHandle", { scaleHandleSize * 2.0f, scaleHandleSize * 2.0f });
        const bool isScaleActive = ImGui::IsItemActive(); // 均一拡縮ハンドルをドラッグ中か
        const bool isScaleActivated = ImGui::IsItemActivated(); // 均一拡縮ハンドルのドラッグを開始したか
        ImGui::PopID();
        if (isScaleActivated) {
            activeGizmoOperationMode_ = 2;
            activeGizmoAxisIndex_ = -1;
        }
        if (isScaleActive) {
            const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
            const float scaleRate = (std::max)(1.0f + (mouseDelta.x - mouseDelta.y) * kGizmoScaleSpeed, 0.01f);
            scale.x = (std::max)(scale.x * scaleRate, kGizmoMinimumScale);
            scale.y = (std::max)(scale.y * scaleRate, kGizmoMinimumScale);
            scale.z = (std::max)(scale.z * scaleRate, kGizmoMinimumScale);
            changed = true;
        }

        if (changed) {
            selectedObject->SetScale(scale);
            if (ctx.notifyObjectTransformEdited) {
                ctx.notifyObjectTransformEdited(static_cast<size_t>(resolvedObjectIndex));
            }
        }
        return;
    }
}

} // namespace MyEngine
#endif