#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer RadialBlurSettings : register(b0) {
    uint gKernelSize;
    uint gDirection;
    float gSigma;
    float gOutlineStrength;
    float2 gBlurCenter;
    float gBlurWidth;
    uint gSampleCount;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input) {
    uint sampleCount =
        clamp(gSampleCount, 1u, 32u); // 安全な範囲へ制限したサンプル数
    float2 direction =
        input.texcoord - gBlurCenter; // 中心から現在のUVへの方向
    float3 accumulatedColor =
        float3(0.0f, 0.0f, 0.0f); // サンプル色の合計

    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        float2 sampleTexcoord =
            input.texcoord -
            direction * gBlurWidth * float(sampleIndex); // 中心方向へずらしたUV
        accumulatedColor +=
            gTexture.Sample(gSampler, sampleTexcoord).rgb;
    }

    float4 sourceColor =
        gTexture.Sample(gSampler, input.texcoord); // 元画像の色とアルファ

    PixelShaderOutput output;
    output.color = float4(
        accumulatedColor / float(sampleCount),
        sourceColor.a);
    return output;
}
