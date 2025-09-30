cbuffer Material : register(b0) {
    float4 gColor;
    uint gEnableLighting;
    float3 _pad0;
    row_major float4x4 gUVTransform;
};

float4 main() : SV_TARGET {
    return gColor;
}
