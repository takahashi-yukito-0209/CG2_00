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
RWStructuredBuffer<int> gFreeCounter : register(u1);

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
        int particleIndex = 0;
        InterlockedAdd(gFreeCounter[0], 1, particleIndex);
        if (particleIndex >= kMaxParticles)
        {
            continue;
        }

        float3 seed = float3(
            float(particleIndex) + float(countIndex),
            gPerFrame.time,
            gPerFrame.deltaTime + float(DTid.x));
        float3 randomScale = Rand3d(seed);
        float3 randomTranslate = Rand3d(seed + randomScale);
        float3 randomColor = Rand3d(seed + randomTranslate);
        float angle = Rand1d(seed + randomColor) * 6.28318530718f;

        Particle particle = (Particle)0;
        particle.scale = float3(0.15f, 0.15f, 0.15f) + randomScale * 0.25f;
        particle.rotate = float3(0.0f, 0.0f, angle);
        particle.translate = gEmitter.translate + (randomTranslate * 2.0f - 1.0f) * gEmitter.radius;
        particle.velocity = (randomTranslate * 2.0f - 1.0f) * 0.5f;
        particle.lifeTime = 1.0f;
        particle.currentTime = 0.0f;
        particle.color = float4(randomColor, 1.0f);
        gParticles[particleIndex] = particle;
    }
}