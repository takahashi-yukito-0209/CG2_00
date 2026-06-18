#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float4 color : SV_TARGET;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float4 textureColor =
        gTexture.Sample(gSampler, input.texcoord); // 元画像の色

    // UV座標を画面中央が原点になる座標へ変換する
    float2 centeredTexcoord =
        input.texcoord * (1.0f - input.texcoord.yx);

    // 画面中央から周辺へ向かって暗くなる係数を求める
    float vignette =
        centeredTexcoord.x * centeredTexcoord.y * 16.0f;
    vignette = saturate(pow(vignette, 0.8f));

    output.color = float4(
        textureColor.rgb * vignette,
        textureColor.a);
    return output;
}
