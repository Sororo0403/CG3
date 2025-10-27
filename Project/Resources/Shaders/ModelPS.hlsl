cbuffer ObjectCB : register(b0) {
    float4x4 gWorld;
    float4 gColor; // tint placeholder
    float gAlphaMul; // 1.0 = solid, 0.0 = invisible
    uint gUseTex; // 0: vertex color, 1: texture
    float2 _pad_;
}
cbuffer SceneCB : register(b1) {
    float4x4 gView;
    float4x4 gProj;
}

Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

struct PSIn {
    float4 svpos : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : POSITION1;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 ModelPS(PSIn i) : SV_TARGET {
    // simple lambert
    float3 L = normalize(float3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normalize(i.nrmW), L), 0.0);

    float4 base = (gUseTex != 0) ? gTex.Sample(gSamp, i.uv) : i.color;

    float3 lit = base.rgb * (0.2 + 0.8 * NdotL);

    float alpha = base.a * gAlphaMul;

    return float4(lit, alpha);
}
