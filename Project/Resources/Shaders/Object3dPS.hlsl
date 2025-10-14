cbuffer MaterialCB : register(b1) {
    float4 gColor;
    int gUseTexture;
    float3 _pad1;
    float4x4 gUvTransform;
}

cbuffer DirLightCB : register(b2) {
    float3 gLightColor;
    float gLightIntensity;
    float3 gLightDir;
    float _pad2; // 「光の向き」= 進行方向（物体→光源は -dir ）
}

Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

struct PSIn {
    float4 posH : SV_Position;
    float3 normalW : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
};

float4 main(PSIn IN) : SV_Target {
    // Lambert
    float3 N = normalize(IN.normalW);
    float3 L = normalize(-gLightDir);
    float diff = saturate(dot(N, L));

    // base color
    float4 texCol = (gUseTexture != 0) ? gTex.Sample(gSamp, IN.uv) : float4(1, 1, 1, 1);
    float3 baseRgb = texCol.rgb * gColor.rgb;

    // simple ambient + diffuse
    float3 ambient = 0.1 * baseRgb;
    float3 diffuse = diff * baseRgb * gLightColor * gLightIntensity;

    float3 rgb = ambient + diffuse;
    float a = texCol.a * gColor.a;

    return float4(rgb, a);
}
