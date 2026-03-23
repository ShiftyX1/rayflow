// Skybox fragment shader — HLSL (SM 5.0)

TextureCube  environmentMap : register(t0);
SamplerState sampler0       : register(s0);

struct PSInput {
    float4 pos     : SV_Position;
    float3 fragDir : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target {
    float3 dir = normalize(input.fragDir);
    float3 rgb = environmentMap.Sample(sampler0, dir).rgb;
    return float4(rgb, 1.0);
}
