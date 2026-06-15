#include "PrimitiveFactory.h"

using namespace MyEngine;

/// <summary>
/// XY平面のPlane頂点データを生成する
/// </summary>
std::vector<Object3d::VertexData> PrimitiveFactory::CreatePlane(float width, float height)
{
    const float halfWidth = width * 0.5f; // 横幅の半分
    const float halfHeight = height * 0.5f; // 縦幅の半分
    const Math::Vector3 normal = { 0.0f, 0.0f, -1.0f }; // 面の法線

    return {
        { { -halfWidth, -halfHeight, 0.0f, 1.0f }, { 0.0f, 1.0f }, normal },
        { { -halfWidth,  halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { {  halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { {  halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { { -halfWidth,  halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { {  halfWidth,  halfHeight, 0.0f, 1.0f }, { 1.0f, 0.0f }, normal },
    };
}
