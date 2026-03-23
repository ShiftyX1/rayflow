// Voxel vertex shader — HLSL (SM 5.0)

cbuffer Params : register(b0) {
    float4x4 mvp;
    float4x4 matModel;
    float4x4 matNormal;
    float4x4 lightSpaceMatrix;
    float3   viewPos;
    float    _pad0;
};

struct VSInput {
    float3 position  : POSITION;
    float2 texCoord  : TEXCOORD0;
    float2 texCoord2 : TEXCOORD1;   // .x = foliageMask, .y = ao
    float3 normal    : NORMAL;
    float4 color     : COLOR;       // rgb = tint, a = skylight
};

struct VSOutput {
    float4 pos            : SV_Position;
    float2 texCoord       : TEXCOORD0;
    float3 tint           : TEXCOORD1;
    float  skyLight       : TEXCOORD2;
    float3 normal         : TEXCOORD3;
    float3 worldPos       : TEXCOORD4;
    float  ao             : TEXCOORD5;
    float  foliageMask    : TEXCOORD6;
    float4 posLightSpace  : TEXCOORD7;
    float  dist           : TEXCOORD8;
};

VSOutput VSMain(VSInput input) {
    VSOutput o;

    o.texCoord    = input.texCoord;
    o.tint        = input.color.rgb;
    o.skyLight    = input.color.a;

    o.normal      = normalize(mul(matNormal, float4(input.normal, 0.0)).xyz);
    o.worldPos    = mul(matModel, float4(input.position, 1.0)).xyz;
    o.ao          = input.texCoord2.y;
    o.foliageMask = input.texCoord2.x;

    o.posLightSpace = mul(lightSpaceMatrix, float4(o.worldPos, 1.0));
    o.dist          = length(o.worldPos - viewPos);

    o.pos = mul(mvp, float4(input.position, 1.0));
    return o;
}
