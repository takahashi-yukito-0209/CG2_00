struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
    float3 worldPosition : POSITION0;

};

// カメラのRight/Upベクトル（フルビルボード用）
cbuffer CameraVectors : register(b2)
{
    float3 gCameraRight;
    float  _pad0;
    float3 gCameraUp;
    float  gBillboardEnable; // 0 or 1
    float4x4 gViewProj;
}
