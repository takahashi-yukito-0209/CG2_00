#include "Particle.hlsli"

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
};

// StructuredBuffer for instancing (vertex shader, t0)
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    // select per-instance data; host must ensure instanceId < allocated count
    ParticleForGPU p = gParticle[instanceId];
    float4 pos = input.position;
    float3 nrm = input.normal;
    if (gBillboardEnable >= 0.5f) {
        // 入力頂点のxyをカメラRight/Upに展開し、中心はWorldの平行移動へ
        float3 center = float3(p.World._41, p.World._42, p.World._43);
        float3 right = normalize(gCameraRight);
        float3 up = normalize(gCameraUp);
        float3 billboardPos = center + right * pos.x + up * pos.y;
        pos = float4(billboardPos, 1.0f);
        // 法線はカメラ方向に向ける（簡易）
        nrm = normalize(cross(right, up));
        // ViewProj を使用してスクリーンへ投影
        output.position = mul(pos, gViewProj);
    } else {
        output.position = mul(pos, p.WVP);
    }
    output.texcoord = input.texcoord;
    output.normal = nrm;
    output.color = p.color;
    return output;
}
