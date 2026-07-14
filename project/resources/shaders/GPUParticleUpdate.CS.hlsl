struct ParticleSource
{
    float3 scale;
    float padding0;
    float3 rotate;
    float padding1;
    float3 translate;
    float padding2;
    float4 color;
};

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4x4 WorldInverseTranspose;
};

cbuffer ParticleTransformInfo : register(b0)
{
    uint gParticleCount;
    float3 gPadding;
    float4x4 gView;
    float4x4 gProjection;
};

StructuredBuffer<ParticleSource> gParticleSources : register(t0);
RWStructuredBuffer<ParticleForGPU> gOutputParticles : register(u0);

float4x4 MakeAffineMatrix(float3 scale, float3 rotate, float3 translate)
{
    float sx = sin(rotate.x);
    float cx = cos(rotate.x);
    float sy = sin(rotate.y);
    float cy = cos(rotate.y);
    float sz = sin(rotate.z);
    float cz = cos(rotate.z);

    float4x4 scaleMatrix = float4x4(
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    float4x4 rotateXMatrix = float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cx, sx, 0.0f,
        0.0f, -sx, cx, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    float4x4 rotateYMatrix = float4x4(
        cy, 0.0f, -sy, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        sy, 0.0f, cy, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    float4x4 rotateZMatrix = float4x4(
        cz, sz, 0.0f, 0.0f,
        -sz, cz, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    float4x4 translateMatrix = float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        translate.x, translate.y, translate.z, 1.0f);

    return mul(mul(mul(mul(scaleMatrix, rotateXMatrix), rotateYMatrix), rotateZMatrix), translateMatrix);
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= gParticleCount)
    {
        return;
    }

    ParticleSource source = gParticleSources[particleIndex];
    float4x4 world = MakeAffineMatrix(source.scale, source.rotate, source.translate);
    float4x4 viewProjection = mul(gView, gProjection);

    ParticleForGPU output;
    output.World = world;
    output.WVP = mul(world, viewProjection);
    output.color = source.color;
    output.WorldInverseTranspose = world;

    gOutputParticles[particleIndex] = output;
}