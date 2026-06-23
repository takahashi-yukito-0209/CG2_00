#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

cbuffer OutlineSettings : register(b0)
{
    float gDepthThreshold;
    float gDepthSoftness;
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

float ConvertToViewZ(float depth)
{
    float4 viewPosition =
        mul(float4(0.0f, 0.0f, depth, 1.0f), gProjectionInverse);
    return viewPosition.z / max(abs(viewPosition.w), 0.0001f);
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint textureWidth = 0; // 入力テクスチャの横幅
    uint textureHeight = 0; // 入力テクスチャの縦幅
    gDepthTexture.GetDimensions(textureWidth, textureHeight);
    float2 uvStepSize =
        rcp(float2(textureWidth, textureHeight)); // 1テクセル分のUV幅

    float centerDepth =
        gDepthTexture.Sample(gSamplerPoint, input.texcoord); // 中心テクセルの深度
    float centerViewZ =
        ConvertToViewZ(centerDepth); // 中心テクセルのView空間深度

    // 背景部分では輪郭を生成しない
    if (centerDepth >= 0.9999f)
    {
        PixelShaderOutput backgroundOutput;
        backgroundOutput.color =
            gTexture.Sample(gSampler, input.texcoord);
        return backgroundOutput;
    }

    float2 difference = float2(0.0f, 0.0f); // 横と縦の相対深度差

    for (int32_t y = 0; y < 3; ++y)
    {
        for (int32_t x = 0; x < 3; ++x)
        {
            float2 offset =
                float2(x - 1, y - 1) * uvStepSize; // 周辺テクセルへの差分
            float depth =
                gDepthTexture.Sample(
                    gSamplerPoint,
                    input.texcoord + offset); // 補間しない深度値
            float relativeDepthDifference = 0.0f; // 距離に依存しにくい相対深度差
            if (depth >= 0.9999f)
            {
                // 地形と背景の境界は明確な輪郭として扱う
                relativeDepthDifference = 1.0f;
            }
            else
            {
                float viewZ = ConvertToViewZ(depth); // 周辺のView空間深度
                relativeDepthDifference =
                    (viewZ - centerViewZ) /
                    max(abs(centerViewZ), 0.0001f);
            }

            difference.x +=
                relativeDepthDifference * kPrewittHorizontalKernel[y][x];
            difference.y +=
                relativeDepthDifference * kPrewittVerticalKernel[y][x];
        }
    }

    float depthGradient = length(difference); // 深度勾配の大きさ
    float detectedEdge =
        smoothstep(
            gDepthThreshold,
            gDepthThreshold + max(gDepthSoftness, 0.0001f),
            depthGradient); // 強度とは独立して輪郭を判定する
    float edgeWeight =
        saturate(detectedEdge * (gOutlineStrength / 6.0f)); // 検出後の濃さだけを調整する
    float4 textureColor =
        gTexture.Sample(gSampler, input.texcoord); // 元画像の色

    PixelShaderOutput output;
    output.color = float4(
        textureColor.rgb * (1.0f - edgeWeight),
        textureColor.a);
    return output;
}
