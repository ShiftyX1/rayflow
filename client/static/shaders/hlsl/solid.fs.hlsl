// Solid color fragment shader — HLSL (SM 5.0)

cbuffer Params : register(b0) {
    float4 color;
};

struct PSInput {
    float4 pos : SV_Position;
};

float4 PSMain(PSInput input) : SV_Target {
    return color;
}
