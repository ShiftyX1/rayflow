// Solid color vertex shader — HLSL (SM 5.0)

cbuffer Params : register(b0) {
    float4x4 mvp;
};

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 pos : SV_Position;
};

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.pos = mul(mvp, float4(input.position, 1.0));
    return o;
}
