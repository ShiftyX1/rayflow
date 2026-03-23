// Tonemap vertex shader — HLSL (SM 5.0)
// Fullscreen triangle

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 pos      : SV_Position;
    float2 texCoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.pos      = float4(input.position, 1.0);
    o.texCoord = input.position.xy * 0.5 + 0.5;
    // Flip Y for DX (top-down texture coordinates)
    o.texCoord.y = 1.0 - o.texCoord.y;
    return o;
}
