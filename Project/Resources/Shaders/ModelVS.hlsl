// ===== cbuffers =====
cbuffer ObjectCB : register(b0) {
    float4x4 gWorld;
    // ティントを使いたい場合だけコメント解除
    // float4 gColor;
};
cbuffer SceneCB : register(b1) {
    float4x4 gView;
    float4x4 gProj;
};

// ===== VS I/O =====
struct VSIn {
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0; // MTL(Kd/d)→頂点に焼いておく
};

struct VSOut {
    float4 svpos : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : POSITION1;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

// ===== VS =====
VSOut ModelVS(VSIn i) {
    VSOut o;

    float4 pw = mul(float4(i.pos, 1.0), gWorld);
    o.svpos = mul(mul(pw, gView), gProj);
    o.posW = pw.xyz;

    // 法線は w=0 でワールドへ
    o.nrmW = mul(float4(i.nrm, 0.0), gWorld).xyz;

    o.uv = i.uv;

    // MTL色（i.col）をそのまま流す
    float4 matColor = i.col;

    // ティントを掛けたい場合は以下を有効化
    // matColor *= gColor;

    o.color = matColor;
    return o;
}
