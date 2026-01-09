struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
};

// Material for pixel shader
struct Material
{
    float4 color;
    int enableLighting;
    float3 _pad0;
    float4x4 uvTransform;
    int lightingMode;
    int useAlphaCutoutSampler;
    float2 _pad1;
    float shininess; // specular power
    float3 _pad2; // pad to 16-byte
};

// Camera (world position only for specular direction)
struct Camera { float3 worldPosition; };
