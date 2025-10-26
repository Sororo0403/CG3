// ===== cbuffers =====
cbuffer ObjectCB : register(b0) {
    float4x4 gWorld;
    float4 gColor; // 使わないなら未使用でOK（将来のティント用）
    uint gUseTex; // 0: 頂点色, 1: テクスチャ
    uint3 _pad_;
}
cbuffer SceneCB : register(b1) {
    float4x4 gView;
    float4x4 gProj;
}

// VS I/O
struct VSIn {
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0; // OBJ/MTLのKd/dを頂点色に焼いたもの（フォールバック用）
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

    // 法線は w=0 でワールドへ
    o.nrmW = mul(float4(i.nrm, 0.0), gWorld).xyz;

    o.uv = i.uv;

    // 頂点色はPSでのフォールバック用
    o.color = i.col;

    return o;
}
