struct PSIn {
    float4 svpos : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : POSITION1;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 PSMain(PSIn i) : SV_TARGET {
    // 超シンプルなディフューズ（平行光: (0.5, 1, 0.3) を上から）
    float3 L = normalize(float3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normalize(i.nrmW), L), 0.0);
    float3 base = i.color.rgb;
    float3 c = base * (0.2 + 0.8 * NdotL);
    return float4(c, i.color.a);
}
