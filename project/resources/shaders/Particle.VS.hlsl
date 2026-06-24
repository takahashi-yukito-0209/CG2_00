#include "Particle.hlsli"

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4x4 WorldInverseTranspose;
};

// StructuredBuffer for instancing (vertex shader, t1)
StructuredBuffer<ParticleForGPU> gParticle : register(t1);

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
    if (gBillboardEnable >= 0.5f)
    {
        // Worldの移動とスケールだけを使い、回転はカメラRight/Upで置き換える
        float3 center = float3(p.World._41, p.World._42, p.World._43);
        float3 right = normalize(gCameraRight);
        float3 up = normalize(gCameraUp);
        float2 localScale = float2(
            length(float3(p.World._11, p.World._12, p.World._13)),
            length(float3(p.World._21, p.World._22, p.World._23)));
        float2 localOffset = input.position.xy * localScale;
        float3 billboardPos = center + right * localOffset.x + up * localOffset.y;
        pos = float4(billboardPos, 1.0f);
        nrm = normalize(cross(right, up));
        output.position = mul(pos, gViewProj);
        output.worldPosition = billboardPos;
        output.normal = nrm;
    }
    else
    {
        output.position = mul(pos, p.WVP);
        output.worldPosition = mul(pos, p.World).xyz;
        output.normal = normalize(mul(nrm, (float3x3) p.WorldInverseTranspose));
    }
    output.texcoord = input.texcoord;
    output.color = p.color;
    return output;
}
