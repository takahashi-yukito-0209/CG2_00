TextureCube<float4> gSky : register(t0);
SamplerState gSampler : register(s0);

struct PSInput { float4 pos : SV_POSITION; float3 dir : TEXCOORD0; };

float4 main(PSInput input) : SV_TARGET0 {
    // sample cubemap using direction passed from VS
    float3 dir = normalize(input.dir);
    float4 color = gSky.Sample(gSampler, dir);
    return color;
}
