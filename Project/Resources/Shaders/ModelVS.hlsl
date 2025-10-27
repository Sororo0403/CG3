cbuffer ObjectCB : register(b0) {
    float4x4 gWorld;
    float4 gColor; // unused tint placeholder
    float gAlphaMul; // not used in VS, but keep layout in sync
    uint gUseTex;
    float2 _pad_;
}
cbuffer SceneCB : register(b1) {
    float4x4 gView;
    float4x4 gProj;
}

struct VSIn {
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0; // baked vertex color / fallback
};

struct VSOut {
    float4 svpos : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : POSITION1;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOut ModelVS(VSIn i) {
    VSOut o;

    float4 pw = mul(float4(i.pos, 1.0), gWorld);
    o.svpos = mul(mul(pw, gView), gProj);
    o.posW = pw.xyz;

    // normal transform (w=0 vector)
    o.nrmW = mul(float4(i.nrm, 0.0), gWorld).xyz;

    o.uv = i.uv;
    o.color = i.col;

    return o;
}
