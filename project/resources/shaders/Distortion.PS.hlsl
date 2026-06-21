#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer DistortionSettings : register(b0)
{
    uint gKernelSize;
    uint gDirection;
    float gSigma;
    float gOutlineStrength;
    float2 gDistortionCenter;
    float gDistortionStrength;
    uint gDistortionPadding0;
    float gDistortionRadius;
    float gDistortionWaveCount;
    float gDistortionProgress;
    float gDistortionPadding1;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    const float aspectRatio = 1280.0f / 720.0f; // 距離計算に使用する画面アスペクト比
    float2 centerToUv = input.texcoord - gDistortionCenter; // 歪み中心から現在のUVへの方向
    float2 aspectDirection = float2(
        centerToUv.x * aspectRatio,
        centerToUv.y); // 画面比率を補正した方向
    float distanceFromCenter = length(aspectDirection); // 歪み中心からの距離
    float safeRadius = max(gDistortionRadius, 0.0001f); // ゼロ除算を避ける影響半径
    float normalizedDistance = distanceFromCenter / safeRadius; // 半径に対する現在位置の距離
    float influence = 1.0f - smoothstep(0.0f, 1.0f, normalizedDistance); // 中心から外側へ減衰する影響度
    float wave = sin(
        normalizedDistance * gDistortionWaveCount * 6.2831853f
        - gDistortionProgress * 6.2831853f); // 外側へ進行する波
    float2 radialDirection = distanceFromCenter > 0.0001f
        ? aspectDirection / distanceFromCenter
        : float2(0.0f, 0.0f); // 中心から外側への正規化方向
    radialDirection.x /= aspectRatio;

    float distortionAmount =
        gDistortionStrength * influence * (0.35f + wave * 0.65f); // 最終的なUV移動量
    float2 distortedUv = saturate(
        input.texcoord + radialDirection * distortionAmount); // 歪ませた参照UV

    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, distortedUv);
    return output;
}
