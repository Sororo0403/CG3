cbuffer Material : register(b0) {
    float4 gColor;
};

cbuffer Transform : register(b1) {
    float4x4 gWVP;
};

struct VSOutput {
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET {
    return gColor;
}
