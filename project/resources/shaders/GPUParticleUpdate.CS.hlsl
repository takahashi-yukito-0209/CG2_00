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
    float3 startScale;
    float padding2;
    float4 startColor;
};

cbuffer ParticleTransformInfo : register(b0)
{
    uint gParticleCount;
    float3 gPadding;
    float4x4 gView;
    float4x4 gProjection;
};

StructuredBuffer<Particle> gParticleSources : register(t0);
RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= gParticleCount)
    {
        return;
    }

    gParticles[particleIndex] = gParticleSources[particleIndex];
}