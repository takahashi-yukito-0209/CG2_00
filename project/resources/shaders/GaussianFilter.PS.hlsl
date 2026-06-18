#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer GaussianFilterSettings : register(b0) {
    uint gKernelSize;
    uint gDirection;
    float gSigma;
    uint gPadding;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input) {
    uint textureWidth = 0; // 入力テクスチャの横幅
    uint textureHeight = 0; // 入力テクスチャの縦幅
    gTexture.GetDimensions(textureWidth, textureHeight);

    float2 uvStepSize = rcp(
        float2(textureWidth, textureHeight)); // 1テクセル分のUV幅
    float2 filterDirection =
        gDirection == 0 ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f); // フィルター方向
    int32_t kernelRadius =
        int32_t(gKernelSize / 2); // 中心から参照するテクセル範囲
    float sigmaSquared =
        max(gSigma * gSigma, 0.0001f); // ゼロ除算を防いだ分散

    float3 filteredColor = float3(0.0f, 0.0f, 0.0f); // 重み付き色の合計
    float weightTotal = 0.0f; // 有限カーネル内の重み合計

    for (int32_t offset = -kernelRadius; offset <= kernelRadius; ++offset) {
        float offsetValue = float(offset); // 中心からの距離
        float weight = exp(
            -(offsetValue * offsetValue) /
            (2.0f * sigmaSquared)); // 1次元Gaussianの重み
        float2 sampleTexcoord =
            input.texcoord +
            filterDirection * offsetValue * uvStepSize; // 参照先のUV座標
        float3 sampleColor =
            gTexture.Sample(gSampler, sampleTexcoord).rgb; // 参照テクセルの色

        filteredColor += sampleColor * weight;
        weightTotal += weight;
    }

    PixelShaderOutput output;
    output.color = float4(
        filteredColor / max(weightTotal, 0.0001f),
        gTexture.Sample(gSampler, input.texcoord).a);
    return output;
}
