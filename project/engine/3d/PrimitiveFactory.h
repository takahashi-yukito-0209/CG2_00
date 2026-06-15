#pragma once
#include "Object3d.h"
#include <vector>

namespace MyEngine {

/// <summary>
/// プリミティブ形状の頂点データを生成するクラス
/// </summary>
class PrimitiveFactory {
public:
    /// <summary>
    /// XY平面のPlane頂点データを生成する
    /// </summary>
    static std::vector<Object3d::VertexData> CreatePlane(float width = 1.0f, float height = 1.0f);

    /// <summary>
    /// XY平面のRing頂点データを生成する
    /// </summary>
    static std::vector<Object3d::VertexData> CreateRing(float outerRadius = 1.0f, float innerRadius = 0.2f, uint32_t divide = 32);
};

} // namespace MyEngine
