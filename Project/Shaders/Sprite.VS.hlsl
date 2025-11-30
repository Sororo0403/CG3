#include "Sprite.hlsli"

cbuffer SpriteCB : register(b0) {
    float4x4 gWVP;
    float4   gColor;
    // uvRect: (u0, v0, uSize, vSize) 0〜1
    float4   gUVRect;
};

SpriteVSOutput main(SpriteVSInput input) {
    SpriteVSOutput o;
    o.pos = mul(float4(input.pos, 1.0f), gWVP);

    // 元の頂点 uv は 0〜1 全面
    float2 uv = input.uv;
    uv = gUVRect.xy + uv * gUVRect.zw;
    o.uv = uv;
    return o;
}