#include "Sprite.hlsli"

cbuffer SpriteCB : register(b0) {
    float4x4 gWVP;
    float4   gColor;
    float4   gUVRect;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(SpriteVSOutput input) : SV_TARGET
{
    float4 tex = gTexture.Sample(gSampler, input.uv);
    return tex * gColor;
}
