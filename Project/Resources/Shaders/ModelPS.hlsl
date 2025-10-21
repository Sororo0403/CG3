struct PSIn {
    float4 svpos : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : POSITION1;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0; // VS からの最終色（Kd/d 反映済）
};

// 超シンプルなディフューズ
float4 ModelPS(PSIn i) : SV_TARGET {
    float3 L = normalize(float3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normalize(i.nrmW), L), 0.0);

    float3 lit = i.color.rgb * (0.2 + 0.8 * NdotL);
    return float4(lit, i.color.a);
}
