#include "Object3d.hlsli"

Texture2D<float4> gtexture : register(t0);
SamplerState gSampler : register(s0);
SamplerState gSamplerPointClamp : register(s1);

cbuffer gMaterial : register(b0)
{
    Material _gMaterial;
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord, 0.0f,1.0f), _gMaterial.uvTransform);
    float4 textureColorLinear = gtexture.Sample(gSampler, transformedUV.xy);
    float4 textureColorPoint = gtexture.Sample(gSamplerPointClamp, transformedUV.xy);
    float4 textureColor = (_gMaterial.useAlphaCutoutSampler != 0) ? textureColorPoint : textureColorLinear;

    float3 finalRGB = _gMaterial.color.rgb * textureColor.rgb;
    float finalA = _gMaterial.color.a * textureColor.a;

    output.color = float4(finalRGB, finalA);
#if !SWAPCHAIN_SRGB
    // Swapchain is UNORM: perform gamma encode here to approximate sRGB output.
    output.color.rgb = pow(output.color.rgb, 1.0/2.2);
#endif
    return output;
}
