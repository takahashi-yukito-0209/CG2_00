static const uint kMaxParticles = 1024;

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

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    gParticles[particleIndex] = (Particle)0;
    gFreeList[particleIndex] = particleIndex;

    if (particleIndex == 0)
    {
        gFreeListIndex[0] = kMaxParticles - 1;
    }
}