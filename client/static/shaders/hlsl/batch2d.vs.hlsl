// Batch2D vertex shader — HLSL (SM 5.0)

cbuffer Params : register(b0) {
    float4x4 uProjection;
};

struct VSInput {
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 pos      : SV_Position;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR;
};

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.texCoord = input.texCoord;
    o.color    = input.color;
    o.pos      = mul(uProjection, float4(input.position, 0.0, 1.0));
    return o;
}
