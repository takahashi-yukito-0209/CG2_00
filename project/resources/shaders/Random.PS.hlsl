#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer RandomSettings : register(b0)
{
    uint gKernelSize;
    uint gDirection;
    float gSigma;
    float gOutlineStrength;
    float2 gBlurCenter;
    float gBlurWidth;
    uint gSampleCount;
    float gThreshold;
    float gEdgeWidth;
    float2 gDissolvePadding;
    float3 gEdgeColor;
    float gDissolvePadding2;
    float gRandomTime;
    float gRandomStrength;
    float gRandomScale;
    float gRandomPadding;
};

float Random2dTo1(float2 value)
{
    float2 randomVector = float2(12.9898f, 78.233f);
    float randomValue =
        sin(dot(value, randomVector)) * 43758.5453f;
    return frac(randomValue);
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 seed =
        floor(input.texcoord * gRandomScale) +
        float2(gRandomTime, gRandomTime * 1.6180339f); // UVと時間からSeedを作る
    float randomValue = Random2dTo1(seed); // 0以上1未満の乱数
    float noiseMultiplier =
        lerp(1.0f, randomValue, gRandomStrength); // ノイズの乗算強度
    float4 textureColor =
        gTexture.Sample(gSampler, input.texcoord); // 入力画像の色

    PixelShaderOutput output;
    output.color = float4(
        textureColor.rgb * noiseMultiplier,
        textureColor.a);
    return output;
}
