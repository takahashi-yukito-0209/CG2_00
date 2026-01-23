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
// Bound at b3 in pixel shader to avoid clashing with particle billboard VS constants
struct Camera { float3 worldPosition; };

// Point light entry for shader-side layout
struct PointLightEntry {
    float4 position; // xyz = position, w = unused
    float4 color;    // rgb = color, w = intensity
    float radius;    // maximum effective range
    float decay;     // falloff exponent
    int enabled;
    float pad;       // pad to 16-byte boundary
};
// Container for binding as a single CBV
// Reduced to support a single point light to match CPU-side configuration
struct PointLightArray {
    PointLightEntry lights[1];
};
