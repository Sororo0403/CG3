#include "Model.hlsli"

ModelVSOutput main(ModelVSInput input)
{
    ModelVSOutput output;
    output.pos = mul(gWVP, float4(input.pos, 1.0f));
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}
