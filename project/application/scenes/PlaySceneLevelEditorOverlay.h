#pragma once

#include "../../engine/level/LevelData.h"
#include "../../engine/utility/MathTypes.h"

namespace LevelEditorOverlay {

/// <summary>
/// Scene View上でLevel Editor用コライダー編集表示を描画し、編集されたかを返す。
/// </summary>
bool DrawColliderOverlay(
    MyEngine::LevelObjectData& objectData,
    int gizmoOperationMode,
    const Math::Matrix4x4& viewProjectionMatrix,
    float imageMinX,
    float imageMinY,
    float imageWidth,
    float imageHeight);

} // namespace LevelEditorOverlay
