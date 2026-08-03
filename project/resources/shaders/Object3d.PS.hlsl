#include "Object3d.hlsli"

Texture2D<float4> gtexture : register(t0);
TextureCube<float4> gEnvironment : register(t2);
SamplerState gSampler : register(s0);
SamplerState gSamplerPointClamp : register(s1);

ConstantBuffer<Material> gMaterial : register(b0);
// Camera CB is bound at b3 (separate from particle billboard CB at b2)
ConstantBuffer<Camera> gCamera : register(b3);

struct DirectionalLight
{
    float4 color; //!< ライトの色
    float3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

// Note: gDirectionalLight is still declared for compatibility, but if particles skip lighting
// it may be left unused. Keep declaration to avoid shader compile errors when PS expects a CBV at b1.
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
// Point lights are declared in Object3d.hlsli as PointLightArray and bound at b4
ConstantBuffer<PointLightArray> gPointLights : register(b4);
// Spot light bound at b5
ConstantBuffer<SpotLightArray> gSpotLight : register(b5);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // Choose sampler based on material flag (useAlphaCutoutSampler).
    // If the material requests alpha-cutout sampler, use point sampling and clamp addressing
    // to avoid bleeding of transparent pixels. Otherwise use linear sampler for smooth filtering.
    // Sampling: some platforms/drivers disallow selecting a sampler resource dynamically.
    // Sample with both samplers and select the result in shader code to avoid that limitation.
    float4 textureColorLinear = gtexture.Sample(gSampler, transformedUV.xy);
    float4 textureColorPoint = gtexture.Sample(gSamplerPointClamp, transformedUV.xy);
    float4 sampledTextureColor = (gMaterial.useAlphaCutoutSampler != 0) ? textureColorPoint : textureColorLinear;
    float4 textureColor = (gMaterial.useTexture != 0) ? sampledTextureColor : float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Separate lighting for RGB and keep alpha controlled by material/texture
    float3 texRGB = textureColor.rgb;
    float texA = textureColor.a;
    float3 matRGB = gMaterial.color.rgb;
    float matA = gMaterial.color.a;

    float3 finalRGB = matRGB * texRGB;
    float finalA = matA * texA;

    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
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
        float3 halfVector = normalize(L + toEye);
        float NdotH = dot(N, halfVector);
        float specularPow = pow(saturate(NdotH), gMaterial.shininess);
        float3 specular = gDirectionalLight.color.rgb * specularPow * gDirectionalLight.intensity;

        // Apply directional light (as before)
        float3 accum = finalRGB * gDirectionalLight.color.rgb * lighting * gDirectionalLight.intensity + specular;

        // Add a small ambient term so objects remain visible if lighting is weak
        const float3 ambient = float3(0.15f, 0.15f, 0.15f);
        accum = accum + finalRGB * ambient;

        // Accumulate single point light (if enabled). Reduced loop for single-light config.
        PointLightEntry pl = gPointLights.lights[0];
        if (pl.enabled != 0)
        {
            float3 toLight = pl.position.xyz - input.worldPosition;
            float dist = length(toLight);
            float range = pl.radius; // use explicit radius
            // proceed only if within range and distance is non-zero
            if (dist <= range && dist > 0.0001f)
            {
                float3 Lp = normalize(toLight);
                float NdotLp = max(dot(N, Lp), 0.0f);

                // distance attenuation: inverse-square bias combined with a smooth range fade
                float d = dist / max(0.0001f, range);
                float attenuation = pow(saturate(1.0f - d), pl.decay);
                attenuation += 0.5f / (1.0f + 0.1f * dist + 0.5f * dist * dist);
                attenuation = saturate(attenuation);

                float3 diffuseContrib = finalRGB * pl.color.rgb * pl.color.w * NdotLp * attenuation;

                float3 halfVec = normalize(Lp + toEye);
                float NdotH = max(dot(N, halfVec), 0.0f);
                float specularPow = pow(saturate(NdotH), gMaterial.shininess);
                float3 specularContrib = pl.color.rgb * specularPow * pl.color.w * attenuation;

                accum += diffuseContrib + specularContrib;
            }
        }

        // Spot light contribution (single spot)
        SpotLightEntry sl = gSpotLight.light;
        if (sl.enabled != 0)
        {
            float3 toSpot = sl.position.xyz - input.worldPosition;
            float distS = length(toSpot);
            if (distS <= sl.distance && distS > 0.0001f)
            {
                float3 Ls = normalize(toSpot);
                float angleCos = dot(Ls, normalize(sl.direction));
                // compute falloff between cosAngle (full) and cosFalloffStart (start fading)
                float falloff = saturate((angleCos - sl.cosFalloffStart) / max(1e-6, sl.cosAngle - sl.cosFalloffStart));
                // distance attenuation (same style as point light)
                float dS = distS / max(0.0001f, sl.distance);
                float attenuationS = pow(saturate(1.0f - dS), sl.decay);
                attenuationS *= falloff;

                float NdotLs = max(dot(N, Ls), 0.0f);
                float3 diffuseS = finalRGB * sl.color.rgb * sl.color.w * NdotLs * attenuationS;

                float3 halfVecS = normalize(Ls + toEye);
                float NdotHS = max(dot(N, halfVecS), 0.0f);
                float specPowS = pow(saturate(NdotHS), gMaterial.shininess);
                float3 specS = sl.color.rgb * specPowS * sl.color.w * attenuationS;

                accum += diffuseS + specS;
            }
        }

        finalRGB = accum;
        // Apply exposure and optional tone mapping to control overall brightness
        // Multiply by exposure first
        finalRGB *= gCamera.exposure;
        // If tone mapping is enabled, apply Reinhard tone mapping
        if (gCamera.toneMapOn != 0)
        {
            finalRGB = finalRGB / (1.0f + finalRGB);
        }

        if (gMaterial.environmentCoefficient > 0.0f && gCamera.hasEnvironmentMap != 0)
        {
            float3 reflected = reflect(-toEye, N);
            float3 environmentDir = normalize(mul(float4(reflected, 0.0f), gCamera.view).xyz);
            float3 environmentRGB = gEnvironment.Sample(gSampler, environmentDir).rgb;
            finalRGB = lerp(finalRGB, environmentRGB, saturate(gMaterial.environmentCoefficient));
        }
    }

    output.color = float4(finalRGB, finalA);
#if !SWAPCHAIN_SRGB
    // If swapchain is not sRGB, encode to approximate sRGB output
    output.color.rgb = pow(output.color.rgb, 1.0 / 2.2);
#endif
    // Binary alpha cutout: discard pixels where the texture's alpha is effectively zero.
    // This implements 2-value (on/off) transparency: fully transparent texels are discarded,
    // opaque texels are rendered normally. Use texture alpha (texA) to decide.
    if (gMaterial.useAlphaDiscard != 0 && texA <= 0.001f)
    {
        discard;
    }

    return output;
}
