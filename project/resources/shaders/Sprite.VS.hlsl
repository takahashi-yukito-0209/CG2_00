#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

cbuffer gTransformationMatrix : register(b0)
{
    TransformationMatrix _gTransformationMatrix;
}

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, _gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    output.worldPosition = mul(input.position, _gTransformationMatrix.World).xyz;
    return output;
}
