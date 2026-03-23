// Tonemap fragment shader — HLSL (SM 5.0)
// Lottes 2016 tone mapping

cbuffer Params : register(b0) {
    float exposure;
    float3 _pad0;
};

Texture2D    hdrBuffer : register(t0);
SamplerState sampler0  : register(s0);

struct PSInput {
    float4 pos      : SV_Position;
    float2 texCoord : TEXCOORD0;
};

float3 lottesTonemap(float3 color) {
    const float a      = 1.4;
    const float d      = 0.977;
    const float hdrMax = 8.0;
    const float midIn  = 0.18;
    const float midOut = 0.18;

    float b = (-pow(midIn, a) + pow(hdrMax, a) * midOut)
            / ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    float c = (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut)
            / ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    return pow(color, a) / (pow(color, a * d) * b + c);
}

float3 linearToSRGB(float3 color) {
    float3 lo = color * 12.92;
    float3 hi = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, color));
}

float4 PSMain(PSInput input) : SV_Target {
    float3 hdrColor = hdrBuffer.Sample(sampler0, input.texCoord).rgb;
    hdrColor *= exposure;
    float3 mapped = lottesTonemap(hdrColor);
    mapped = clamp(mapped, 0.0, 1.0);

    // Vignette (subtle)
    float2 uv = input.texCoord * 2.0 - 1.0;
    float vignette = 1.0 - dot(uv, uv) * 0.15;
    mapped *= vignette;

    float3 srgb = linearToSRGB(mapped);
    return float4(srgb, 1.0);
}
