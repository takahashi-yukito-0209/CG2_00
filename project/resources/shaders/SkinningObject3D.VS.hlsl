#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4x4 WorldInverseTranspose;
};

struct WellForGPU
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
StructuredBuffer<WellForGPU> gMatrixPalette : register(t3);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : BLENDWEIGHT0;
    int4 index : BLENDINDICES0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex)
    {
        int jointIndex = input.index[influenceIndex];
        float weight = input.weight[influenceIndex];
        skinnedPosition += mul(input.position, gMatrixPalette[jointIndex].skeletonSpaceMatrix) * weight;
        skinnedNormal += mul(input.normal, (float3x3) gMatrixPalette[jointIndex].skeletonSpaceInverseTransposeMatrix) * weight;
    }

    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    return output;
}