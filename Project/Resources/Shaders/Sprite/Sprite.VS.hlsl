#include "Sprite.hlsli"

cbuffer SpriteCB : register(b0)
{
    float4x4 gWVP;
    float4 gColor;
    float4 gUVRect;
};

SpriteVSOutput main(SpriteVSInput input)
{
    SpriteVSOutput output;

    output.pos = mul(gWVP, float4(input.pos, 1.0f));

    float2 uv = input.uv;
    uv = gUVRect.xy + uv * gUVRect.zw;
    output.uv = uv;

    return output;
}
