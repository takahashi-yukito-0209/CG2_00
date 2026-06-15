#include "PrimitiveFactory.h"
#include <cmath>
#include <numbers>

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

/// <summary>
/// XY平面のRing頂点データを生成する
/// </summary>
std::vector<Object3d::VertexData> PrimitiveFactory::CreateRing(float outerRadius, float innerRadius, uint32_t divide)
{
    std::vector<Object3d::VertexData> vertices; // 生成する頂点データ
    vertices.reserve(static_cast<size_t>(divide) * 6);

    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divide); // 1分割あたりの角度
    const Math::Vector3 normal = { 0.0f, 0.0f, -1.0f }; // 面の法線

    for (uint32_t index = 0; index < divide; ++index) {
        const float sin = std::sin(static_cast<float>(index) * radianPerDivide); // 現在角のsin
        const float cos = std::cos(static_cast<float>(index) * radianPerDivide); // 現在角のcos
        const float sinNext = std::sin(static_cast<float>(index + 1) * radianPerDivide); // 次角のsin
        const float cosNext = std::cos(static_cast<float>(index + 1) * radianPerDivide); // 次角のcos
        const float u = static_cast<float>(index) / static_cast<float>(divide); // 現在角のU座標
        const float uNext = static_cast<float>(index + 1) / static_cast<float>(divide); // 次角のU座標

        Object3d::VertexData outerCurrent { { -sin * outerRadius, cos * outerRadius, 0.0f, 1.0f }, { u, 0.0f }, normal }; // 外側の現在頂点
        Object3d::VertexData outerNext { { -sinNext * outerRadius, cosNext * outerRadius, 0.0f, 1.0f }, { uNext, 0.0f }, normal }; // 外側の次頂点
        Object3d::VertexData innerCurrent { { -sin * innerRadius, cos * innerRadius, 0.0f, 1.0f }, { u, 1.0f }, normal }; // 内側の現在頂点
        Object3d::VertexData innerNext { { -sinNext * innerRadius, cosNext * innerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, normal }; // 内側の次頂点

        vertices.push_back(outerCurrent);
        vertices.push_back(innerCurrent);
        vertices.push_back(outerNext);

        vertices.push_back(innerCurrent);
        vertices.push_back(innerNext);
        vertices.push_back(outerNext);
    }

    return vertices;
}
