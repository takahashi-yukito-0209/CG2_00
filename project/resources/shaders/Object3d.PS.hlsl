#include "Object3d.hlsli"

Texture2D<float4> gtexture : register(t0);
SamplerState gSampler : register(s0);
SamplerState gSamplerPointClamp : register(s1);

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);

struct DirectionalLight
{
    float4 color; //!< ライトの色
    float3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

// Note: gDirectionalLight is still declared for compatibility, but if particles skip lighting
// it may be left unused. Keep declaration to avoid shader compile errors when PS expects a CBV at b1.
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord, 0.0f,1.0f), gMaterial.uvTransform);
    // Choose sampler based on material flag (useAlphaCutoutSampler).
    // If the material requests alpha-cutout sampler, use point sampling and clamp addressing
    // to avoid bleeding of transparent pixels. Otherwise use linear sampler for smooth filtering.
    // Sampling: some platforms/drivers disallow selecting a sampler resource dynamically.
    // Sample with both samplers and select the result in shader code to avoid that limitation.
    float4 textureColorLinear = gtexture.Sample(gSampler, transformedUV.xy);
    float4 textureColorPoint = gtexture.Sample(gSamplerPointClamp, transformedUV.xy);
    float4 textureColor = (gMaterial.useAlphaCutoutSampler != 0) ? textureColorPoint : textureColorLinear;
    
    // Separate lighting for RGB and keep alpha controlled by material/texture
    float3 texRGB = textureColor.rgb;
    float  texA = textureColor.a;
    float3 matRGB = gMaterial.color.rgb;
    float  matA = gMaterial.color.a;

    float3 finalRGB = matRGB * texRGB;
    float  finalA = matA * texA;

    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float NdotL = dot(N, L);
        float lighting = 1.0f;

        if (gMaterial.lightingMode == 1)
        {
            // Lambert
            lighting = max(NdotL, 0.0f);
        }
        else if (gMaterial.lightingMode == 2)
        {
            // Half-Lambert
            lighting = pow(NdotL * 0.5f + 0.5f, 2.0f);
        }

        // Specular: reflect light vector around normal and compute view direction
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        float3 halfVector = normalize(L + toEye);
        float NdotH = dot(N, halfVector);
        float specularPow = pow(saturate(NdotH), gMaterial.shininess);
        float3 specular = gDirectionalLight.color.rgb * specularPow * gDirectionalLight.intensity;

        // Apply lighting only to RGB channels
        finalRGB = finalRGB * gDirectionalLight.color.rgb * lighting * gDirectionalLight.intensity + specular;
    }

    output.color = float4(finalRGB, finalA);
    // Binary alpha cutout: discard pixels where the texture's alpha is effectively zero.
    // This implements 2-value (on/off) transparency: fully transparent texels are discarded,
    // opaque texels are rendered normally. Use texture alpha (texA) to decide.
    if (texA <= 0.001f) {
        discard;
    }

    return output;
}