#include "Fullscreen.hlsli"

VertexShaderOutput main(uint vertexId : SV_VertexID) {
    VertexShaderOutput output;

    // 全画面を覆う三角形の頂点座標
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(3.0f, -1.0f),
        float2(-1.0f, 3.0f)
    };

    // DirectXのテクスチャ座標系に合わせたUV座標
    float2 texcoords[3] = {
        float2(0.0f, 1.0f),
        float2(2.0f, 1.0f),
        float2(0.0f, -1.0f)
    };

    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.texcoord = texcoords[vertexId];
    return output;
}
