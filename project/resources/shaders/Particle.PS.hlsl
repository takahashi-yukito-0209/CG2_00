#include "Particle.hlsli"

Texture2D<float4> gtexture : register(t0);
SamplerState gSampler : register(s0);
SamplerState gSamplerPointClamp : register(s1);

struct Material
{
    float4 color;
    int enableLighting;
    float3 _pad0;
    float4x4 uvTransform;
    int lightingMode;
    int useAlphaCutoutSampler;
    float2 _pad1;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColorLinear = gtexture.Sample(gSampler, transformedUV.xy);
    float4 textureColorPoint = gtexture.Sample(gSamplerPointClamp, transformedUV.xy);
    float4 textureColor = (gMaterial.useAlphaCutoutSampler != 0) ? textureColorPoint : textureColorLinear;

    float3 texRGB = textureColor.rgb;
    float texA = textureColor.a;
    float3 matRGB = gMaterial.color.rgb;
    float matA = gMaterial.color.a;

    float3 finalRGB = matRGB * texRGB;
    float finalA = matA * texA;

    if (gMaterial.enableLighting != 0)
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float lighting = 1.0f;

        if (gMaterial.lightingMode == 1)
        {
            lighting = max(NdotL, 0.0f);
        }
        else if (gMaterial.lightingMode == 2)
        {
            lighting = pow(NdotL * 0.5f + 0.5f, 2.0f);
        }

        finalRGB = finalRGB * gDirectionalLight.color.rgb * lighting * gDirectionalLight.intensity;
    }

    output.color = float4(finalRGB, finalA);
    if (texA <= 0.001f) { discard; }
    return output;
}
