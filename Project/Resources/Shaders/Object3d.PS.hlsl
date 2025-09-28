#include "Object3d.hlsli"

struct Material {
    float4 color;
    int enableLighting;
    float3 padding;
    float4x4 uvTransform;
};

struct DirectionalLight {
    float4 color;
    float3 direction;
    float intensity;
    int mode;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gLight : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    float4 baseColor = textureColor * gMaterial.color;

    if (gMaterial.enableLighting != 0 && gLight.mode != 0) {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gLight.direction);

        float lambert = 1.0f;

        if (gLight.mode == 1) {
            lambert = max(dot(N, L), 0.0f);
        }
        else if (gLight.mode == 2) {
            lambert = pow(dot(N, L) * 0.5f + 0.5f, 2.0f);
        }

        float3 light = gLight.color.rgb * gLight.intensity * lambert;
        output.color = float4(baseColor.rgb * light, baseColor.a);
    }
    else {
        output.color = baseColor;
    }

    return output;
}
