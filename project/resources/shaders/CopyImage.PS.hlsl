#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput { float4 color : SV_TARGET; };

PSOutput main(VertexShaderOutput input) {
    PSOutput o;
    o.color = gTexture.Sample(gSampler, input.texcoord);
    return o;
}
