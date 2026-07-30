#include "PlaySceneLevelEditorOverlay.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../../engine/utility/mathUtility.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace Math;

namespace {

#ifdef USE_IMGUI
/// <summary>
/// 2D座標同士の差分を作成する。
/// </summary>
ImVec2 SubtractImVec2(const ImVec2& lhs, const ImVec2& rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y };
}

/// <summary>
/// 2Dベクトルの長さを取得する。
/// </summary>
float GetImVec2Length(const ImVec2& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}

/// <summary>
/// 2Dベクトルの内積を取得する。
/// </summary>
float DotImVec2(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

/// <summary>
/// Scene View画像の編集領域が操作可能なサイズか判定する。
/// </summary>
bool IsLevelEditorSceneViewRectUsable(const ImVec2& imageSize)
{
    constexpr float kMinimumSceneViewImageSize = 4.0f; // Scene View操作を許可する最小表示サイズ
    return std::isfinite(imageSize.x)
        && std::isfinite(imageSize.y)
        && imageSize.x >= kMinimumSceneViewImageSize
        && imageSize.y >= kMinimumSceneViewImageSize;
}

/// <summary>
/// ワールド座標をScene View上の座標へ投影する。
/// </summary>
bool ProjectLevelEditorWorldToSceneView(const Vector3& worldPosition, const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, ImVec2& outScreenPosition)
{
    const float clipX = worldPosition.x * viewProjectionMatrix.m[0][0] + worldPosition.y * viewProjectionMatrix.m[1][0] + worldPosition.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0]; // クリップ座標X
    const float clipY = worldPosition.x * viewProjectionMatrix.m[0][1] + worldPosition.y * viewProjectionMatrix.m[1][1] + worldPosition.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1]; // クリップ座標Y
    const float clipZ = worldPosition.x * viewProjectionMatrix.m[0][2] + worldPosition.y * viewProjectionMatrix.m[1][2] + worldPosition.z * viewProjectionMatrix.m[2][2] + viewProjectionMatrix.m[3][2]; // クリップ座標Z
    const float clipW = worldPosition.x * viewProjectionMatrix.m[0][3] + worldPosition.y * viewProjectionMatrix.m[1][3] + worldPosition.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3]; // 透視除算に使うW
    if (!std::isfinite(clipW) || std::fabs(clipW) <= 0.000001f) {
        return false;
    }

    const float ndcX = clipX / clipW; // NDC座標X
    const float ndcY = clipY / clipW; // NDC座標Y
    const float ndcZ = clipZ / clipW; // NDC座標Z
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
        return false;
    }

    outScreenPosition.x = imageMin.x + (ndcX + 1.0f) * 0.5f * imageSize.x;
    outScreenPosition.y = imageMin.y + (1.0f - (ndcY + 1.0f) * 0.5f) * imageSize.y;
    return ndcZ >= 0.0f && ndcZ <= 1.0f;
}

/// <summary>
/// 回転行列で方向ベクトルだけを変換する。
/// </summary>
Vector3 TransformLevelEditorDirection(const Vector3& direction, const Matrix4x4& matrix)
{
    Vector3 result {}; // 変換後の方向
    result.x = direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0];
    result.y = direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1];
    result.z = direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2];
    return MathUtil::SafeNormalize(result, direction);
}

/// <summary>
/// 行列で方向ベクトルを長さを保ったまま変換する。
/// </summary>
Vector3 TransformLevelEditorVector(const Vector3& direction, const Matrix4x4& matrix)
{
    Vector3 result {}; // 変換後のベクトル
    result.x = direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0];
    result.y = direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1];
    result.z = direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2];
    return result;
}

/// <summary>
/// Scene View座標をビュー射影逆行列でワールド座標へ戻す。
/// </summary>
Vector3 UnprojectLevelEditorSceneViewPoint(const ImVec2& screenPosition, float ndcDepth, const Matrix4x4& inverseViewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize)
{
    const float screenRateX = imageSize.x != 0.0f ? (screenPosition.x - imageMin.x) / imageSize.x : 0.5f; // 画像内X割合
    const float screenRateY = imageSize.y != 0.0f ? (screenPosition.y - imageMin.y) / imageSize.y : 0.5f; // 画像内Y割合
    const Vector3 ndcPosition {
        screenRateX * 2.0f - 1.0f,
        1.0f - screenRateY * 2.0f,
        ndcDepth,
    }; // 逆変換に使うNDC座標
    return MathUtil::Transform(ndcPosition, inverseViewProjectionMatrix);
}

/// <summary>
/// Scene Viewのマウス位置から指定平面上のワールド座標を計算する。
/// </summary>
bool CalculateLevelEditorMousePlanePoint(const ImVec2& screenPosition, const Matrix4x4& inverseViewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, const Vector3& planePoint, const Vector3& planeNormal, Vector3& outWorldPoint)
{
    const Vector3 rayStart = UnprojectLevelEditorSceneViewPoint(screenPosition, 0.0f, inverseViewProjectionMatrix, imageMin, imageSize); // Near側のレイ位置
    const Vector3 rayEnd = UnprojectLevelEditorSceneViewPoint(screenPosition, 1.0f, inverseViewProjectionMatrix, imageMin, imageSize); // Far側のレイ位置
    const Vector3 rayDirection = rayEnd - rayStart; // マウス位置から伸びるワールドレイ
    const float denominator = MathUtil::Dot(rayDirection, planeNormal); // レイと平面の交差判定用分母
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float distance = MathUtil::Dot(planePoint - rayStart, planeNormal) / denominator; // レイ上の交点距離
    outWorldPoint = rayStart + rayDirection * distance;
    return std::isfinite(outWorldPoint.x) && std::isfinite(outWorldPoint.y) && std::isfinite(outWorldPoint.z);
}

/// <summary>
/// Scene Viewのマウスドラッグ量をカメラ平面上のワールド移動量へ変換する。
/// </summary>
bool CalculateLevelEditorViewPlaneWorldDelta(const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, const Vector3& planePoint, Vector3& outWorldDelta)
{
    outWorldDelta = {}; // 変換できない場合の移動量
    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
    if (std::fabs(mouseDelta.x) <= 0.000001f && std::fabs(mouseDelta.y) <= 0.000001f) {
        return false;
    }

    if (!IsLevelEditorSceneViewRectUsable(imageSize)) {
        return false;
    }

    const Matrix4x4 inverseViewProjectionMatrix = MathUtil::Inverse(viewProjectionMatrix); // ワールド座標へ戻す逆ビュー射影行列
    const ImVec2 imageCenter { imageMin.x + imageSize.x * 0.5f, imageMin.y + imageSize.y * 0.5f }; // Scene Viewの中心座標
    const Vector3 centerRayStart = UnprojectLevelEditorSceneViewPoint(imageCenter, 0.0f, inverseViewProjectionMatrix, imageMin, imageSize); // 中心レイのNear位置
    const Vector3 centerRayEnd = UnprojectLevelEditorSceneViewPoint(imageCenter, 1.0f, inverseViewProjectionMatrix, imageMin, imageSize); // 中心レイのFar位置
    const Vector3 planeNormal = MathUtil::SafeNormalize(centerRayEnd - centerRayStart, { 0.0f, 0.0f, 1.0f }); // カメラ平面の法線

    const ImVec2 currentMousePosition = ImGui::GetIO().MousePos; // 現在のマウス座標
    const ImVec2 previousMousePosition { currentMousePosition.x - mouseDelta.x, currentMousePosition.y - mouseDelta.y }; // 前フレームのマウス座標
    Vector3 previousWorldPoint {}; // 前フレームの平面上座標
    Vector3 currentWorldPoint {}; // 現在の平面上座標
    if (!CalculateLevelEditorMousePlanePoint(previousMousePosition, inverseViewProjectionMatrix, imageMin, imageSize, planePoint, planeNormal, previousWorldPoint)
        || !CalculateLevelEditorMousePlanePoint(currentMousePosition, inverseViewProjectionMatrix, imageMin, imageSize, planePoint, planeNormal, currentWorldPoint)) {
        return false;
    }

    outWorldDelta = currentWorldPoint - previousWorldPoint;
    return MathUtil::LengthSquared(outWorldDelta) > 0.0000000001f;
}

/// <summary>
/// BOXコライダー中心の自由移動ハンドルを描画してドラッグ量を返す。
/// </summary>
bool DrawLevelColliderCenterMoveHandle(const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, const ImVec2& centerScreen, const Vector3& worldCenter, Vector3& outWorldDelta)
{
    outWorldDelta = {}; // ハンドルから得たワールド移動量
    ImDrawList* drawList = ImGui::GetWindowDrawList(); // Scene Viewへ描画するDrawList
    constexpr float kCenterHandleRadius = 8.0f; // 中心自由移動ハンドルの半径
    drawList->AddCircleFilled(centerScreen, kCenterHandleRadius, IM_COL32(255, 245, 120, 240), 16);
    drawList->AddCircle(centerScreen, kCenterHandleRadius + 2.0f, IM_COL32(20, 20, 20, 210), 16, 2.0f);

    ImGui::SetCursorScreenPos({ centerScreen.x - kCenterHandleRadius, centerScreen.y - kCenterHandleRadius });
    ImGui::PushID("LevelColliderMoveCenter");
    ImGui::InvisibleButton("LevelColliderMoveCenter", { kCenterHandleRadius * 2.0f, kCenterHandleRadius * 2.0f });
    const bool isCenterActive = ImGui::IsItemActive(); // 中心自由移動ハンドルをドラッグ中か
    const bool isCenterHovered = ImGui::IsItemHovered(); // 中心自由移動ハンドルにマウスが乗っているか
    ImGui::PopID();

    if (isCenterHovered || isCenterActive) {
        drawList->AddCircle(centerScreen, kCenterHandleRadius + 5.0f, IM_COL32(255, 255, 255, 255), 18, 2.0f);
    }
    if (!isCenterActive) {
        return false;
    }

    return CalculateLevelEditorViewPlaneWorldDelta(viewProjectionMatrix, imageMin, imageSize, worldCenter, outWorldDelta);
}

/// <summary>
/// BOXコライダー中心の均一拡縮ハンドルを描画して倍率を返す。
/// </summary>
bool DrawLevelColliderUniformScaleHandle(const ImVec2& centerScreen, float* outScaleRate)
{
    if (!outScaleRate) {
        return false;
    }

    *outScaleRate = 1.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList(); // Scene Viewへ描画するDrawList
    constexpr float kScaleHandleHalfSize = 8.0f; // 均一拡縮ハンドルの半分サイズ
    constexpr float kScaleSpeed = 0.01f; // 均一拡縮のドラッグ感度
    const ImVec2 handleMin { centerScreen.x - kScaleHandleHalfSize, centerScreen.y - kScaleHandleHalfSize }; // ハンドル左上
    const ImVec2 handleMax { centerScreen.x + kScaleHandleHalfSize, centerScreen.y + kScaleHandleHalfSize }; // ハンドル右下
    drawList->AddRectFilled(handleMin, handleMax, IM_COL32(255, 245, 120, 240), 2.0f);
    drawList->AddRect(handleMin, handleMax, IM_COL32(20, 20, 20, 210), 2.0f, 0, 2.0f);

    ImGui::SetCursorScreenPos(handleMin);
    ImGui::PushID("LevelColliderScaleUniform");
    ImGui::InvisibleButton("LevelColliderScaleUniform", { kScaleHandleHalfSize * 2.0f, kScaleHandleHalfSize * 2.0f });
    const bool isScaleActive = ImGui::IsItemActive(); // 均一拡縮ハンドルをドラッグ中か
    const bool isScaleHovered = ImGui::IsItemHovered(); // 均一拡縮ハンドルにマウスが乗っているか
    ImGui::PopID();

    if (isScaleHovered || isScaleActive) {
        drawList->AddRect({ handleMin.x - 3.0f, handleMin.y - 3.0f }, { handleMax.x + 3.0f, handleMax.y + 3.0f }, IM_COL32(255, 255, 255, 255), 2.0f, 0, 2.0f);
    }
    if (!isScaleActive) {
        return false;
    }

    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
    *outScaleRate = (std::max)(1.0f + (mouseDelta.x - mouseDelta.y) * kScaleSpeed, 0.01f);
    return std::fabs(*outScaleRate - 1.0f) > 0.000001f;
}

/// <summary>
/// BOXコライダーのScene View軸ハンドルを描画してドラッグ量を返す。
/// </summary>
bool DrawLevelColliderSceneGizmoAxis(const char* id, const ImVec2& centerScreen, const ImVec2& handleScreen, ImU32 color, float axisWorldLength, float* outWorldDelta)
{
    if (!outWorldDelta) {
        return false;
    }

    *outWorldDelta = 0.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList(); // Scene View上へ描画するDrawList
    const ImVec2 axisVector = SubtractImVec2(handleScreen, centerScreen); // 画面上の軸ベクトル
    const float axisPixelLength = GetImVec2Length(axisVector); // 画面上の軸長
    if (axisPixelLength <= 8.0f || axisWorldLength <= 0.000001f) {
        return false;
    }

    const ImVec2 axisDirection = { axisVector.x / axisPixelLength, axisVector.y / axisPixelLength }; // 正規化済み画面軸
    drawList->AddLine(centerScreen, handleScreen, IM_COL32(10, 10, 10, 190), 5.0f);
    drawList->AddLine(centerScreen, handleScreen, color, 3.0f);
    drawList->AddCircleFilled(handleScreen, 6.5f, color, 16);
    drawList->AddCircle(handleScreen, 8.5f, IM_COL32(255, 255, 255, 230), 16, 1.5f);

    ImGui::SetCursorScreenPos({ handleScreen.x - 11.0f, handleScreen.y - 11.0f });
    ImGui::PushID(id);
    ImGui::InvisibleButton("LevelColliderAxisHandle", { 22.0f, 22.0f });
    const bool isActive = ImGui::IsItemActive(); // このハンドルをドラッグ中か
    const bool isHovered = ImGui::IsItemHovered(); // このハンドルにマウスが乗っているか
    ImGui::PopID();

    if (isHovered || isActive) {
        drawList->AddCircle(handleScreen, 12.0f, IM_COL32(255, 255, 255, 255), 18, 2.0f);
    }
    if (!isActive) {
        return false;
    }

    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
    const float pixelDelta = DotImVec2(mouseDelta, axisDirection); // 軸方向の画面移動量
    *outWorldDelta = pixelDelta / axisPixelLength * axisWorldLength;
    return std::fabs(*outWorldDelta) > 0.000001f;
}
#endif

} // namespace

namespace LevelEditorOverlay {

/// <summary>
/// Scene View上でLevel Editor用コライダー編集表示を描画し、編集されたかを返す。
/// </summary>
bool DrawColliderOverlay(
    MyEngine::LevelObjectData& objectData,
    int gizmoOperationMode,
    const Matrix4x4& viewProjectionMatrix,
    float imageMinX,
    float imageMinY,
    float imageWidth,
    float imageHeight)
{
#ifdef USE_IMGUI
    const ImVec2 imageMin { imageMinX, imageMinY }; // Scene View画像の左上座標
    const ImVec2 imageSize { imageWidth, imageHeight }; // Scene View画像の表示サイズ
    const Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(objectData.transform.scale, objectData.transform.rotate, objectData.transform.translate); // LevelObjectのワールド行列
    const Vector3 worldCenter = MathUtil::Transform(objectData.collider.center, worldMatrix); // コライダー中心のワールド座標
    ImVec2 centerScreen {}; // コライダー中心のScene View座標
    if (!ProjectLevelEditorWorldToSceneView(worldCenter, viewProjectionMatrix, imageMin, imageSize, centerScreen)) {
        return false;
    }

    const Matrix4x4 rotateMatrix = MathUtil::Multiply(
        MathUtil::MakeRotateXMatrix(objectData.transform.rotate.x),
        MathUtil::Multiply(MathUtil::MakeRotateYMatrix(objectData.transform.rotate.y), MathUtil::MakeRotateZMatrix(objectData.transform.rotate.z))); // LevelObjectの回転行列
    const std::array<Vector3, 3> localAxes {
        Vector3 { 1.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 1.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 1.0f },
    }; // コライダー編集用のローカル軸
    const std::array<Vector3, 3> worldAxes {
        TransformLevelEditorDirection(localAxes[0], rotateMatrix),
        TransformLevelEditorDirection(localAxes[1], rotateMatrix),
        TransformLevelEditorDirection(localAxes[2], rotateMatrix),
    }; // コライダー編集用のワールド軸
    const std::array<float, 3> axisScales {
        (std::max)(std::fabs(objectData.transform.scale.x), 0.0001f),
        (std::max)(std::fabs(objectData.transform.scale.y), 0.0001f),
        (std::max)(std::fabs(objectData.transform.scale.z), 0.0001f),
    }; // ローカル量とワールド量の変換に使うスケール
    const std::array<ImU32, 3> axisColors {
        IM_COL32(240, 80, 80, 255),
        IM_COL32(80, 220, 110, 255),
        IM_COL32(90, 150, 255, 255),
    }; // XYZハンドル色
    const std::array<const char*, 3> axisIds { "ColliderX", "ColliderY", "ColliderZ" }; // ImGui ID用の軸名

    bool edited = false; // コライダーが変更されたか
    constexpr float kMinimumColliderSize = 0.001f; // コライダーサイズの最小値
    constexpr float kColliderGizmoAxisWorldLength = 1.0f; // コライダーサイズに影響されないギズモ軸長
    const float centerAxisWorldLength = kColliderGizmoAxisWorldLength; // ハンドルまでの固定ワールド距離

    if (gizmoOperationMode == 2) {
        float uniformScaleRate = 1.0f; // 中心ハンドルから得た均一拡縮倍率
        if (DrawLevelColliderUniformScaleHandle(centerScreen, &uniformScaleRate)) {
            objectData.collider.size.x = (std::max)(objectData.collider.size.x * uniformScaleRate, kMinimumColliderSize);
            objectData.collider.size.y = (std::max)(objectData.collider.size.y * uniformScaleRate, kMinimumColliderSize);
            objectData.collider.size.z = (std::max)(objectData.collider.size.z * uniformScaleRate, kMinimumColliderSize);
            edited = true;
        }
    }

    if (gizmoOperationMode == 0) {
        Vector3 centerWorldDelta {}; // 中心ハンドルから得たワールド移動量
        if (DrawLevelColliderCenterMoveHandle(viewProjectionMatrix, imageMin, imageSize, centerScreen, worldCenter, centerWorldDelta)) {
            const Matrix4x4 inverseRotateMatrix = MathUtil::Inverse(rotateMatrix); // ワールド移動量をローカル方向へ戻す逆回転行列
            const Vector3 centerLocalDelta = TransformLevelEditorVector(centerWorldDelta, inverseRotateMatrix); // スケール適用前のローカル移動量
            if (std::fabs(objectData.transform.scale.x) > 0.0001f) {
                objectData.collider.center.x += centerLocalDelta.x / objectData.transform.scale.x;
            }
            if (std::fabs(objectData.transform.scale.y) > 0.0001f) {
                objectData.collider.center.y += centerLocalDelta.y / objectData.transform.scale.y;
            }
            if (std::fabs(objectData.transform.scale.z) > 0.0001f) {
                objectData.collider.center.z += centerLocalDelta.z / objectData.transform.scale.z;
            }
            edited = true;
        }
    }

    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        const float handleWorldLength = centerAxisWorldLength; // コライダーサイズに影響されないハンドル距離
        const Vector3 handleWorld = worldCenter + worldAxes[axisIndex] * handleWorldLength; // ハンドルのワールド座標
        ImVec2 handleScreen {}; // ハンドルのScene View座標
        if (!ProjectLevelEditorWorldToSceneView(handleWorld, viewProjectionMatrix, imageMin, imageSize, handleScreen)) {
            continue;
        }

        float worldDelta = 0.0f; // ハンドルドラッグから得たワールド移動量
        if (!DrawLevelColliderSceneGizmoAxis(axisIds[axisIndex], centerScreen, handleScreen, axisColors[axisIndex], handleWorldLength, &worldDelta)) {
            continue;
        }

        const float localDelta = worldDelta / axisScales[axisIndex]; // LevelDataへ反映するローカル量
        if (gizmoOperationMode == 0) {
            if (axisIndex == 0) {
                objectData.collider.center.x += localDelta;
            } else if (axisIndex == 1) {
                objectData.collider.center.y += localDelta;
            } else {
                objectData.collider.center.z += localDelta;
            }
        } else {
            if (axisIndex == 0) {
                objectData.collider.size.x = (std::max)(objectData.collider.size.x + localDelta * 2.0f, kMinimumColliderSize);
            } else if (axisIndex == 1) {
                objectData.collider.size.y = (std::max)(objectData.collider.size.y + localDelta * 2.0f, kMinimumColliderSize);
            } else {
                objectData.collider.size.z = (std::max)(objectData.collider.size.z + localDelta * 2.0f, kMinimumColliderSize);
            }
        }
        edited = true;
    }

    return edited;
#else
    (void)objectData;
    (void)gizmoOperationMode;
    (void)viewProjectionMatrix;
    (void)imageMinX;
    (void)imageMinY;
    (void)imageWidth;
    (void)imageHeight;
    return false;
#endif
}

} // namespace LevelEditorOverlay
