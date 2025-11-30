struct SpriteVSInput {
    float3 pos  : POSITION0;
    float2 uv   : TEXCOORD0;
};

struct SpriteVSOutput {
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;
};
