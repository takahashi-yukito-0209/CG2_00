#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

cbuffer DissolveSettings : register(b0)
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
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float maskValue =
        gMaskTexture.Sample(gSampler, input.texcoord); // ノイズマスクの値

    if (maskValue < gThreshold)
    {
        discard;
    }

    float edge =
        1.0f - smoothstep(
            gThreshold,
            gThreshold + max(gEdgeWidth, 0.001f),
            maskValue); // 閾値付近だけを境界として抽出する
    float4 textureColor =
        gTexture.Sample(gSampler, input.texcoord); // 元画像の色

    PixelShaderOutput output;
    output.color = float4(
        textureColor.rgb + edge * gEdgeColor,
        1.0f);
    return output;
}
