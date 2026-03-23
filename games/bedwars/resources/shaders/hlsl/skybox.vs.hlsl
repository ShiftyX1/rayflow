// Skybox vertex shader — HLSL (SM 5.0)

cbuffer Params : register(b0) {
    float4x4 mvp;
};

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 pos    : SV_Position;
    float3 fragDir : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.fragDir = input.position;
    float4 pos = mul(mvp, float4(input.position, 1.0));
    o.pos = pos.xyww; // depth = w/w = 1.0 (far plane)
    return o;
}
