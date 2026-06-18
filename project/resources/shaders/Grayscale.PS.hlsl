#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor =
        gTexture.Sample(gSampler, input.texcoord); // 元画像の色

    // BT.709の輝度係数からグレイスケール値を求める
    const float3 luminanceWeight =
        float3(0.2125f, 0.7154f, 0.0721f);
    float grayscaleValue =
        dot(textureColor.rgb, luminanceWeight); // 輝度値

    output.color = float4(grayscaleValue.xxx, textureColor.a);
    return output;
}
