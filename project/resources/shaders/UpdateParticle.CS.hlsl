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

struct PerFrame
{
    float time;
    float deltaTime;
    uint scaleOverLife;
    uint colorOverLife;
    float3 gravity;
    float damping;
    float3 endScale;
    float padding0;
    float4 endColor;
};

ConstantBuffer<PerFrame> gPerFrame : register(b2);
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

    Particle particle = gParticles[particleIndex];
    if (particle.color.a <= 0.0f)
    {
        return;
    }

    particle.velocity += gPerFrame.gravity * gPerFrame.deltaTime;
    if (gPerFrame.damping > 0.0f)
    {
        float dampingRate = saturate(1.0f - gPerFrame.damping * gPerFrame.deltaTime);
        particle.velocity *= dampingRate;
    }

    particle.translate += particle.velocity * gPerFrame.deltaTime;
    particle.currentTime += gPerFrame.deltaTime;

    float lifeTime = max(particle.lifeTime, 0.0001f);
    float lifeRate = saturate(particle.currentTime / lifeTime);
    float alpha = 1.0f - lifeRate;

    if (gPerFrame.scaleOverLife != 0)
    {
        particle.scale = lerp(particle.startScale, gPerFrame.endScale, lifeRate);
    }

    if (gPerFrame.colorOverLife != 0)
    {
        particle.color = lerp(particle.startColor, gPerFrame.endColor, lifeRate);
    }
    else
    {
        particle.color.a = saturate(alpha);
    }

    if (particle.color.a <= 0.0f)
    {
        particle.scale = float3(0.0f, 0.0f, 0.0f);

        int freeListIndex = 0;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if ((freeListIndex + 1) < kMaxParticles)
        {
            gFreeList[freeListIndex + 1] = particleIndex;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        }
    }

    gParticles[particleIndex] = particle;
}