cbuffer ModelCB : register(b0)
{
    float4x4 gWVP;
};

struct ModelVSInput
{
    float3 pos : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
};

struct ModelVSOutput
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
};