// Batch2D pixel shader — HLSL (SM 5.0)

Texture2D    tex      : register(t0);
SamplerState sampler0 : register(s0);

struct PSInput {
    float4 pos      : SV_Position;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR;
};

float4 PSMain(PSInput input) : SV_Target {
    return tex.Sample(sampler0, input.texCoord) * input.color;
}
