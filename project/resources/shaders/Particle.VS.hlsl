#include "Particle.hlsli"

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4x4 WorldInverseTranspose;
};

// StructuredBuffer for instancing (vertex shader, t1)
StructuredBuffer<ParticleForGPU> gParticle : register(t1);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    // select per-instance data; host must ensure instanceId < allocated count
    ParticleForGPU p = gParticle[instanceId];
    float4 pos = input.position;
    float3 nrm = input.normal;
    if (gBillboardEnable >= 0.5f)
    {
        // 入力頂点のxyをカメラRight/Upに展開し、中心はWorldの平行移動へ
        float3 center = float3(p.World._41, p.World._42, p.World._43);
        float3 right = normalize(gCameraRight);
        float3 up = normalize(gCameraUp);
        float2 localOffset = mul(float4(input.position.xyz, 0.0f), p.World).xy;
        float3 billboardPos = center + right * localOffset.x + up * localOffset.y;
        pos = float4(billboardPos, 1.0f);
        // 法線はカメラ方向に向ける（簡易）
        nrm = normalize(cross(right, up));
        // ViewProj を使用してスクリーンへ投影
        output.position = mul(pos, gViewProj);
        // ワールド座標は billboard 計算後の位置
        output.worldPosition = billboardPos;
    }
    else
    {
        output.position = mul(pos, p.WVP);
        // 非ビルボード時は通常のワールド変換で座標を出力
        output.worldPosition = mul(pos, p.World).xyz;
    }
    output.texcoord = input.texcoord;
    // 法線はインスタンス毎の行列で変換（逆転置行列を使用）
    output.normal = normalize(mul(nrm, (float3x3) p.WorldInverseTranspose));
    output.color = p.color;
    return output;
}
