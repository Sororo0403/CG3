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

float4 main(VertexShaderOutput input) : SV_TARGET {
    float3 baseColor = gMaterial.color.rgb;

    float3 N = normalize(input.normal);
    if (!all(isfinite(N)) || length(N) < 1e-4f) {
        N = float3(0.0f, 1.0f, 0.0f);
    }

    float3 lighting = baseColor * 0.15f;

    if (gMaterial.enableLighting != 0 && gLight.mode != 0) {
        float3 L = normalize(-gLight.direction);

        float NdotL = saturate(dot(N, L));

        float diffuse = 0.0f;
        if (gLight.mode == 1) {
            diffuse = NdotL;
        }
        else if (gLight.mode == 2) {
            diffuse = pow(NdotL * 0.5f + 0.5f, 2.0f);
        }

        lighting += baseColor * diffuse * gLight.color.rgb * gLight.intensity;
    }

    return float4(lighting, 1.0f);
}
