struct VertexShaderoutput
{
    float32_t4 position : SV_POSITION;
};

struct VertexShaderInput
{
    float32_t4 position : POSITION;
};

VertexShaderoutput main(VertexShaderInput input)
{
    VertexShaderoutput output;
    output.position = input.position;
    return output;
}