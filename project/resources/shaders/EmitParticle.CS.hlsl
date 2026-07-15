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

struct EmitterSphere
{
    float3 translate;
    float radius;
    uint count;
    float frequency;
    float frequencyTime;
    uint emit;
    float3 baseScale;
    float randomScale;
    float3 velocityScale;
    float lifeTime;
    float4 colorMin;
    float4 colorMax;
    uint debugGridMode;
    float3 padding;
};

struct PerFrame
{
    float time;
    float deltaTime;
    float2 padding;
};

ConstantBuffer<EmitterSphere> gEmitter : register(b1);
ConstantBuffer<PerFrame> gPerFrame : register(b2);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

float Rand1d(float3 seed)
{
    return frac(sin(dot(seed, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}

float3 Rand3d(float3 seed)
{
    return float3(
        Rand1d(seed + float3(0.0f, 0.0f, 0.0f)),
        Rand1d(seed + float3(19.19f, 7.31f, 3.17f)),
        Rand1d(seed + float3(5.13f, 23.47f, 11.89f)));
}

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    for (uint countIndex = 0; countIndex < gEmitter.count; ++countIndex)
    {
        int freeListIndex = 0;
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        if (freeListIndex < 0 || freeListIndex >= kMaxParticles)
        {
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
            break;
        }

        uint particleIndex = gFreeList[freeListIndex];
        float3 seed = float3(
            float(particleIndex) + float(countIndex),
            gPerFrame.time,
            gPerFrame.deltaTime + float(DTid.x));
        float3 randomScale = Rand3d(seed);
        float3 randomTranslate = Rand3d(seed + randomScale);
        float3 randomColor = Rand3d(seed + randomTranslate);
        float angle = Rand1d(seed + randomColor) * 6.28318530718f;

        Particle particle = (Particle)0;
        if (gEmitter.debugGridMode != 0)
        {
            uint gridX = particleIndex % 32;
            uint gridY = particleIndex / 32;
            float2 gridPosition = (float2(gridX, gridY) - float2(15.5f, 15.5f)) * max(gEmitter.radius, 0.05f) * 0.25f;
            particle.scale = gEmitter.baseScale;
            particle.rotate = float3(0.0f, 0.0f, 0.0f);
            particle.translate = gEmitter.translate + float3(gridPosition.x, gridPosition.y, 0.0f);
            particle.velocity = float3(0.0f, 0.0f, 0.0f);
            particle.lifeTime = max(gEmitter.lifeTime, 0.0001f);
            particle.currentTime = 0.0f;
            particle.color = gEmitter.colorMax;
        }
        else
        {
            particle.scale = gEmitter.baseScale + randomScale * gEmitter.randomScale;
            particle.rotate = float3(0.0f, 0.0f, angle);
            particle.translate = gEmitter.translate + (randomTranslate * 2.0f - 1.0f) * gEmitter.radius;
            particle.velocity = (randomTranslate * 2.0f - 1.0f) * gEmitter.velocityScale;
            particle.lifeTime = max(gEmitter.lifeTime, 0.0001f);
            particle.currentTime = 0.0f;
            particle.color = lerp(gEmitter.colorMin, gEmitter.colorMax, float4(randomColor, Rand1d(seed + angle)));
        }
        gParticles[particleIndex] = particle;
    }
}
