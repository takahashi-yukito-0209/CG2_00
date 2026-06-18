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
    float environmentCoefficient;
    float2 _pad2; // pad to 16-byte
};

// Camera (world position + exposure/tone mapping control)
// Bound at b3 in pixel shader to avoid clashing with particle billboard VS constants
struct Camera
{
    float3 worldPosition;
    float exposure; // exposure multiplier applied before tone mapping
    int toneMapOn; // 0 = off, non-zero = apply tone mapping
    float pad0;
    float2 pad1;
    float4x4 view;
};

// Point light entry for shader-side layout
struct PointLightEntry
{
    float4 position; // xyz = position, w = unused
    float4 color; // rgb = color, w = intensity
    float radius; // maximum effective range
    float decay; // falloff exponent
    int enabled;
    float pad; // pad to 16-byte boundary
};
// Container for binding as a single CBV
// Reduced to support a single point light to match CPU-side configuration
struct PointLightArray
{
    PointLightEntry lights[1];
};

// Spot light entry for shader-side layout
struct SpotLightEntry
{
    float4 position; // xyz = position, w = unused
    float4 color; // rgb = color, w = intensity
    float3 direction; // spot direction (normalized)
    float distance; // maximum effective range
    float decay; // falloff exponent
    float cosAngle; // cosine of inner cone angle (center)
    float cosFalloffStart; // cosine of falloff start angle
    int enabled; // 0 = disabled, non-zero = enabled
    float pad0; // pad to 16 bytes
};

// Single spot light container
struct SpotLightArray
{
    SpotLightEntry light;
};
