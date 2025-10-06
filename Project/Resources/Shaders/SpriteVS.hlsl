cbuffer Material : register(b0) {
    float4 gColor;
};
cbuffer Transform : register(b1) {
    float4x4 gWVP;
};

struct VSInput {
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};
struct VSOutput {
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.posH = mul(gWVP, float4(input.pos, 1.0f));
    output.uv = input.uv;
    return output;
}
