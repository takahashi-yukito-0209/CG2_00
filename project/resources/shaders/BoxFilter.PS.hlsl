#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BoxFilterSettings : register(b0)
{
    uint gKernelSize;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint textureWidth = 0; // 入力テクスチャの横幅
    uint textureHeight = 0; // 入力テクスチャの縦幅
    gTexture.GetDimensions(textureWidth, textureHeight);

    float2 uvStepSize = rcp(
        float2(textureWidth, textureHeight)); // 1テクセル分のUV幅
    int32_t kernelRadius =
        int32_t(gKernelSize / 2); // 中心から参照するテクセル範囲
    float kernelWeight =
        rcp(float(gKernelSize * gKernelSize)); // 各テクセルの平均化係数

    PixelShaderOutput output;
    output.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    for (int32_t y = -kernelRadius; y <= kernelRadius; ++y)
    {
        for (int32_t x = -kernelRadius; x <= kernelRadius; ++x)
        {
            float2 sampleTexcoord =
                input.texcoord + float2(x, y) * uvStepSize; // 周辺テクセルのUV
            float3 sampleColor =
                gTexture.Sample(gSampler, sampleTexcoord).rgb; // 周辺テクセルの色
            output.color.rgb += sampleColor * kernelWeight;
        }
    }

    return output;
}
