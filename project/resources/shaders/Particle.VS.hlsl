#include "Particle.hlsli"

struct Particle
{
    float3 scale;
    float lifeTime;
    float3 rotate;
    float currentTime;
    float3 translate;
    float padding0;
    float3 velocity;
    float padding1;
    float4 color;
};

StructuredBuffer<Particle> gParticles : register(t1);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    Particle particle = gParticles[instanceId];

    float sinZ = sin(particle.rotate.z);
    float cosZ = cos(particle.rotate.z);
    float2 scaledLocal = input.position.xy * particle.scale.xy;
    float2 rotatedLocal = float2(
        scaledLocal.x * cosZ - scaledLocal.y * sinZ,
        scaledLocal.x * sinZ + scaledLocal.y * cosZ);

    float3 worldPosition;
    float3 worldNormal;
    if (gBillboardEnable >= 0.5f)
    {
        float3 right = normalize(gCameraRight);
        float3 up = normalize(gCameraUp);
        worldPosition = particle.translate + right * rotatedLocal.x + up * rotatedLocal.y;
        worldNormal = normalize(cross(right, up));
    }
    else
    {
        float3 localPosition = float3(rotatedLocal, input.position.z * particle.scale.z);
        worldPosition = localPosition + particle.translate;
        worldNormal = normalize(float3(
            input.normal.x * cosZ - input.normal.y * sinZ,
            input.normal.x * sinZ + input.normal.y * cosZ,
            input.normal.z));
    }

    output.position = mul(float4(worldPosition, 1.0f), gViewProj);
    output.texcoord = input.texcoord;
    output.normal = worldNormal;
    output.color = particle.color;
    output.worldPosition = worldPosition;
    return output;
}