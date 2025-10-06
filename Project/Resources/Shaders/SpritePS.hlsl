cbuffer Material : register(b0) {
    float4 gColor;
};

Texture2D gTex0 : register(t0);
SamplerState gSamp0 : register(s0);

struct VSOutput {
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET {
    float4 tex = gTex0.Sample(gSamp0, input.uv);
    return tex * gColor;
}
