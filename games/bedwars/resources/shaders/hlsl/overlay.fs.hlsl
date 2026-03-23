// Overlay fragment shader — HLSL (SM 5.0)

cbuffer Params : register(b0) {
    float4 tintColor;
};

Texture2D    tex      : register(t0);
SamplerState sampler0 : register(s0);

struct PSInput {
    float4 pos      : SV_Position;
    float2 texCoord : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target {
    float4 texel = tex.Sample(sampler0, input.texCoord);
    if (texel.a < 0.01)
        discard;
    return texel * tintColor;
}
