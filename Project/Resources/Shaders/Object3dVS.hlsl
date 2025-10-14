// ===== Constant Buffers =====
cbuffer TransformCB : register(b0) {
    float4x4 gWorld;
    float4x4 gViewProj;
    float3 gCameraPos;
    float _pad0;
}

cbuffer MaterialCB : register(b1) {
    float4 gColor; // RGBA
    int gUseTexture; // 0/1
    float3 _pad1;
    float4x4 gUvTransform; // 使わないなら単位行列で
}

// ===== VS I/O =====
struct VSIn {
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOut {
    float4 posH : SV_Position;
    float3 normalW : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
};

VSOut main(VSIn IN) {
    VSOut OUT;

    // world
    float4 wp = mul(float4(IN.pos, 1.0f), gWorld);
    OUT.worldPos = wp.xyz;

    // normal（非一様スケールが強い場合は逆転置を使うのが正解だが簡略）
    float3 nW = normalize(mul(float4(IN.normal, 0.0f), gWorld).xyz);
    OUT.normalW = nW;

    // uv（必要なら変換。未使用ならそのまま）
    float4 uvh = mul(float4(IN.uv, 0.0f, 1.0f), gUvTransform);
    OUT.uv = uvh.xy;

    // clip space
    OUT.posH = mul(wp, gViewProj);

    return OUT;
}
