#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer OutlineSettings : register(b0)
{
    uint gKernelSize;
    uint gDirection;
    float gSigma;
    float gOutlineStrength;
    float4x4 gProjectionInverse;
};

static const float kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

static const float kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
};

float Luminance(float3 color)
{
    return dot(color, float3(0.2125f, 0.7154f, 0.0721f));
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint textureWidth = 0; // 入力テクスチャの横幅
    uint textureHeight = 0; // 入力テクスチャの縦幅
    gTexture.GetDimensions(textureWidth, textureHeight);
    float2 uvStepSize =
        rcp(float2(textureWidth, textureHeight)); // 1テクセル分のUV幅

    float2 difference = float2(0.0f, 0.0f); // 横と縦の輝度差

    for (int32_t y = 0; y < 3; ++y)
    {
        for (int32_t x = 0; x < 3; ++x)
        {
            float2 offset =
                float2(x - 1, y - 1) * uvStepSize; // 周辺テクセルへの差分
            float3 sampleColor =
                gTexture.Sample(gSampler, input.texcoord + offset).rgb; // 周辺色
            float luminance = Luminance(sampleColor); // 周辺色の輝度

            difference.x += luminance * kPrewittHorizontalKernel[y][x];
            difference.y += luminance * kPrewittVerticalKernel[y][x];
        }
    }

    float edgeWeight =
        saturate(length(difference) * gOutlineStrength); // 輪郭の強さ
    float4 textureColor =
        gTexture.Sample(gSampler, input.texcoord); // 元画像の色

    PixelShaderOutput output;
    output.color = float4(
        textureColor.rgb * (1.0f - edgeWeight),
        textureColor.a);
    return output;
}
