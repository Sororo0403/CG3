cbuffer ObjectCB : register(b0) {
    float4x4 gWorld;
    float4 gColor;
};
cbuffer SceneCB : register(b1) {
    float4x4 gView;
    float4x4 gProj;
};

struct VSIn {
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOut {
    float4 svpos : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : POSITION1;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float4 pw = mul(float4(i.pos, 1), gWorld);
    float4 pv = mul(pw, gView);
    float4 pp = mul(pv, gProj);
    o.svpos = pp;
    o.posW = pw.xyz;
    o.nrmW = mul(float4(i.nrm, 0), gWorld).xyz;
    o.uv = i.uv;
    o.color = gColor;
    return o;
}
