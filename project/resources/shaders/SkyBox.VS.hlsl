cbuffer VP : register(b0)
{
    row_major float4x4 gVP;
};

struct VSInput { float3 pos : POSITION; };
struct VSOutput { float4 pos : SV_POSITION; float3 dir : TEXCOORD0; };

VSOutput main(VSInput input) {
    VSOutput o;
    // direction vector for cubemap sampling (in world/view space)
    o.dir = input.pos;
    // transform to clip space using VP matrix (translation should already be removed on CPU side)
    o.pos = mul(float4(input.pos, 1.0f), gVP);
    return o;
}
