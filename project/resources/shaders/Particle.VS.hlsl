#include "Particle.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

// StructuredBuffer for instancing (vertex shader, t0)
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    // select per-instance transform; host must ensure instanceId < allocated count
    TransformationMatrix tm = gTransformationMatrices[instanceId];
    output.position = mul(input.position, tm.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3)tm.WorldInverseTranspose));
    output.worldPosition = mul(input.position, tm.World).xyz;
    return output;
}
