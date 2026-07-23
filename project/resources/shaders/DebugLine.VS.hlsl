struct TransformationMatrix
{
    float4x4 WVP;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float3 position : POSITION0;
    float4 color : COLOR0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(float4(input.position, 1.0f), gTransformationMatrix.WVP);
    output.color = input.color;
    return output;
}
