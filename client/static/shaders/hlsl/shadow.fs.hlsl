// Shadow depth pass — fragment shader — HLSL (SM 5.0)

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

struct PSInput {
    float4 pos      : SV_Position;
    float2 texCoord : TEXCOORD0;
};

void PSMain(PSInput input) {
    float alpha = texture0.Sample(sampler0, input.texCoord).a;
    if (alpha < 0.5)
        discard;
    // Depth is written automatically by the rasterizer
}
