struct VSIn {
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer Transform : register(b1) {
    row_major float4x4 gWVP;
    row_major float4x4 gWorld;
};

struct VSOut {
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut main(VSIn i) {
    VSOut o;
    float4 wpos = mul(float4(i.pos, 1), gWorld);
    o.posH = mul(wpos, gWVP);
    o.uv = i.uv;
    return o;
}
