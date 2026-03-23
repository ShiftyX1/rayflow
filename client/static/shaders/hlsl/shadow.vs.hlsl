// Shadow depth pass — vertex shader — HLSL (SM 5.0)

cbuffer Params : register(b0) {
    float4x4 lightSpaceMatrix;
};

struct VSInput {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float2 texCoord2: TEXCOORD1;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 pos      : SV_Position;
    float2 texCoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.texCoord = input.texCoord;
    o.pos = mul(lightSpaceMatrix, float4(input.position, 1.0));
    return o;
}
