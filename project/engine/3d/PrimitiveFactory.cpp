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
        { { -halfWidth, halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { { halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { { halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { { -halfWidth, halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { { halfWidth, halfHeight, 0.0f, 1.0f }, { 1.0f, 0.0f }, normal },
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

/// <summary>
/// Y方向に伸びるCylinderの頂点データを生成する
/// </summary>
std::vector<Object3d::VertexData> PrimitiveFactory::CreateCylinder(float topRadius, float bottomRadius, float height, uint32_t divide)
{
    std::vector<Object3d::VertexData> vertices; // 生成する頂点データ
    vertices.reserve(static_cast<size_t>(divide) * 6);

    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divide); // 1分割あたりの角度
    for (uint32_t index = 0; index < divide; ++index) {
        const float sin = std::sin(static_cast<float>(index) * radianPerDivide); // 現在角度のsin
        const float cos = std::cos(static_cast<float>(index) * radianPerDivide); // 現在角度のcos
        const float sinNext = std::sin(static_cast<float>(index + 1) * radianPerDivide); // 次角度のsin
        const float cosNext = std::cos(static_cast<float>(index + 1) * radianPerDivide); // 次角度のcos
        const float u = static_cast<float>(index) / static_cast<float>(divide); // 現在角度のU座標
        const float uNext = static_cast<float>(index + 1) / static_cast<float>(divide); // 次角度のU座標

        const Math::Vector3 normal = { -sin, 0.0f, cos }; // 現在頂点の外向き法線
        const Math::Vector3 normalNext = { -sinNext, 0.0f, cosNext }; // 次頂点の外向き法線

        Object3d::VertexData topCurrent { { -sin * topRadius, height, cos * topRadius, 1.0f }, { u, 1.0f }, normal }; // 上側の現在頂点
        Object3d::VertexData topNext { { -sinNext * topRadius, height, cosNext * topRadius, 1.0f }, { uNext, 1.0f }, normalNext }; // 上側の次頂点
        Object3d::VertexData bottomCurrent { { -sin * bottomRadius, 0.0f, cos * bottomRadius, 1.0f }, { u, 0.0f }, normal }; // 下側の現在頂点
        Object3d::VertexData bottomNext { { -sinNext * bottomRadius, 0.0f, cosNext * bottomRadius, 1.0f }, { uNext, 0.0f }, normalNext }; // 下側の次頂点

        vertices.push_back(topCurrent);
        vertices.push_back(topNext);
        vertices.push_back(bottomCurrent);

        vertices.push_back(bottomCurrent);
        vertices.push_back(topNext);
        vertices.push_back(bottomNext);
    }

    return vertices;
}
