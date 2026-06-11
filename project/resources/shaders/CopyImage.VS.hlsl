#include "CopyImage.hlsli"

VertexShaderOutput main(uint vertexId : SV_VertexID) {
    VertexShaderOutput o;
    // Full-screen triangle positions
    float2 pos[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    float2 uv[3]  = { float2(0.0, 1.0), float2(2.0, 1.0), float2(0.0, -1.0) };
    o.position = float4(pos[vertexId], 0.0, 1.0);
    o.texcoord = uv[vertexId];
    return o;
}
