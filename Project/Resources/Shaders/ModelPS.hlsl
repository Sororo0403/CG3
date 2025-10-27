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

// テクスチャ＆サンプラ
Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

// PS I/O
struct PSIn {
    float4 svpos : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : POSITION1;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

// 超シンプルなディフューズ
float4 ModelPS(PSIn i) : SV_TARGET {
    float3 L = normalize(float3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normalize(i.nrmW), L), 0.0);

    // テクスチャ or 頂点色
    float4 base = (gUseTex != 0) ? gTex.Sample(gSamp, i.uv) : i.color;

    // 簡易Lambert（環境0.2 + 直射0.8）
    float3 lit = base.rgb * (0.2 + 0.8 * NdotL);
    return float4(lit, base.a);
}
