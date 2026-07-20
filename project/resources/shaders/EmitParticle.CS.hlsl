static const uint kMaxParticles = 16384;

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
    uint spawnShape;
    float3 padding;
    float3 endScale;
    float damping;
    float3 gravity;
    uint scaleOverLife;
    float4 endColor;
    uint colorOverLife;
    float3 padding2;
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

static const uint kSpawnShapeSphere = 0;
static const uint kSpawnShapeBox = 1;
static const uint kSpawnShapeRing = 2;
static const uint kSpawnShapeCone = 3;
static const float kTwoPi = 6.28318530718f;
static const float kMinimumLength = 0.0001f;

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

uint HashUint(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float Random01FromUint(uint seed)
{
    return (float) (HashUint(seed) & 0x00ffffffu) / 16777215.0f;
}

float3 Random3FromUint(uint seed)
{
    return float3(
        Random01FromUint(seed ^ 0x68bc21ebu),
        Random01FromUint(seed ^ 0x02e5be93u),
        Random01FromUint(seed ^ 0x967a889bu));
}

float3 NormalizeOrUp(float3 value)
{
    float valueLength = length(value);
    if (valueLength <= kMinimumLength)
    {
        return float3(0.0f, 1.0f, 0.0f);
    }

    return value / valueLength;
}

void BuildSphereSpawn(float3 randomTranslate, float3 randomVelocity, float radiusSeed, out float3 spawnOffset, out float3 velocityDirection)
{
    spawnOffset = randomTranslate * 2.0f - 1.0f;
    spawnOffset = NormalizeOrUp(spawnOffset) * pow(radiusSeed, 0.3333333f);
    velocityDirection = NormalizeOrUp(randomVelocity * 2.0f - 1.0f);
}

void BuildBoxSpawn(float3 randomTranslate, float3 randomVelocity, out float3 spawnOffset, out float3 velocityDirection)
{
    spawnOffset = randomTranslate * 2.0f - 1.0f;
    velocityDirection = NormalizeOrUp(randomVelocity * 2.0f - 1.0f);
}

void BuildRingSpawn(float angle, float radiusSeed, float heightSeed, out float3 spawnOffset, out float3 velocityDirection)
{
    float ringRadius = sqrt(radiusSeed);
    float2 ringDirection = float2(cos(angle), sin(angle));
    spawnOffset = float3(ringDirection.x * ringRadius, (heightSeed - 0.5f) * 0.1f, ringDirection.y * ringRadius);
    velocityDirection = NormalizeOrUp(float3(ringDirection.x, 0.15f, ringDirection.y));
}

void BuildConeSpawn(float angle, float heightSeed, float radiusSeed, out float3 spawnOffset, out float3 velocityDirection)
{
    float heightRate = saturate(heightSeed);
    float coneRadius = (1.0f - heightRate) * sqrt(radiusSeed);
    float2 coneDirection = float2(cos(angle), sin(angle));
    spawnOffset = float3(coneDirection.x * coneRadius, heightRate, coneDirection.y * coneRadius);
    velocityDirection = NormalizeOrUp(float3(coneDirection.x * coneRadius, 1.0f, coneDirection.y * coneRadius));
}

void BuildSpawnState(float3 seed, float3 randomTranslate, float3 randomVelocity, float angle, out float3 spawnOffset, out float3 velocityDirection)
{
    float radiusSeed = Rand1d(seed + float3(1301.0f, 1409.0f, 1511.0f));
    float heightSeed = Rand1d(seed + float3(1601.0f, 1709.0f, 1801.0f));

    if (gEmitter.spawnShape == kSpawnShapeBox)
    {
        BuildBoxSpawn(randomTranslate, randomVelocity, spawnOffset, velocityDirection);
        return;
    }

    if (gEmitter.spawnShape == kSpawnShapeRing)
    {
        BuildRingSpawn(angle, radiusSeed, heightSeed, spawnOffset, velocityDirection);
        return;
    }

    if (gEmitter.spawnShape == kSpawnShapeCone)
    {
        BuildConeSpawn(angle, heightSeed, radiusSeed, spawnOffset, velocityDirection);
        return;
    }

    BuildSphereSpawn(randomTranslate, randomVelocity, radiusSeed, spawnOffset, velocityDirection);
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    uint countIndex = DTid.x;
    if (countIndex >= gEmitter.count)
    {
        return;
    }

    int freeListIndex = 0;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= kMaxParticles)
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint particleIndex = gFreeList[freeListIndex];
    uint timeSeed = (uint) (gPerFrame.time * 1000.0f);
    uint baseSeed = HashUint(particleIndex ^ (countIndex * 747796405u) ^ (timeSeed * 2891336453u));
    float3 seed = Random3FromUint(baseSeed) * 4096.0f;
    float3 randomScale = Random3FromUint(baseSeed ^ 0x1234abcdu);
    float3 randomTranslate = Random3FromUint(baseSeed ^ 0x9e3779b9u);
    float3 randomVelocity = Random3FromUint(baseSeed ^ 0x85ebca6bu);
    float3 randomColor = Random3FromUint(baseSeed ^ 0xc2b2ae35u);
    float angle = Random01FromUint(baseSeed ^ 0x27d4eb2fu) * kTwoPi;
    float3 spawnOffset = float3(0.0f, 0.0f, 0.0f);
    float3 velocityDirection = float3(0.0f, 1.0f, 0.0f);
    float3 randomVelocityDirection = NormalizeOrUp(float3(randomVelocity.x * 2.0f - 1.0f, randomVelocity.y * 2.0f - 1.0f, (randomVelocity.z * 2.0f - 1.0f) * 0.25f));
    BuildSpawnState(seed, randomTranslate, randomVelocity, angle, spawnOffset, velocityDirection);

    Particle particle = (Particle)0;
    particle.scale = gEmitter.baseScale + randomScale * gEmitter.randomScale;
    particle.rotate = float3(0.0f, 0.0f, angle);
    particle.translate = gEmitter.translate + spawnOffset * gEmitter.radius;
    particle.velocity = randomVelocityDirection * gEmitter.velocityScale;
    particle.lifeTime = max(gEmitter.lifeTime, 0.0001f);
    particle.currentTime = 0.0f;
    particle.color = lerp(gEmitter.colorMin, gEmitter.colorMax, float4(randomColor, Rand1d(seed + angle)));
    particle.startScale = particle.scale;
    particle.startColor = particle.color;
    gParticles[particleIndex] = particle;
}

