// Voxel fragment shader — HLSL (SM 5.0)
//
// Features:
//   - Directional sun light with shadow mapping (PCF soft shadows)
//   - Blinn-Phong specular highlights
//   - Point lights with attenuation
//   - Per-vertex ambient occlusion
//   - Sky light + block light dual lightmap
//   - Distance fog
//   - HDR output

#define MAX_LIGHTS 32

struct PointLight {
    float3 position;
    float  radius;
    float3 color;
    float  intensity;
};

cbuffer Params : register(b0) {
    float4 colDiffuse;

    float3 sunDir;
    float  _pad0;
    float3 sunColor;
    float  _pad1;
    float3 ambientColor;
    float  _pad2;

    float3 fogColor;
    float  fogStart;
    float  fogEnd;
    int    shadowEnabled;
    float2 _pad3;

    float3 viewPos;
    int    lightCount;

    PointLight lights[MAX_LIGHTS];
};

Texture2D    texture0  : register(t0);
SamplerState sampler0  : register(s0);

Texture2D    shadowMap     : register(t1);
SamplerState shadowSampler : register(s1);

struct PSInput {
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

float calcShadow(float4 posLightSpace) {
    float3 projCoords = posLightSpace.xyz / posLightSpace.w;

    // GL: projCoords * 0.5 + 0.5 maps from [-1,1] -> [0,1]
    // DX clip space: X,Y already [-1,1], Z is [0,1]
    projCoords.x = projCoords.x *  0.5 + 0.5;
    projCoords.y = projCoords.y * -0.5 + 0.5; // flip Y (DX top-down)

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    float shadow = 0.0;
    float w, h;
    shadowMap.GetDimensions(w, h);
    float2 texelSize = 1.0 / float2(w, h);

    [unroll]
    for (int x = -1; x <= 1; ++x) {
        [unroll]
        for (int y = -1; y <= 1; ++y) {
            float2 offset = float2((float)x, (float)y) * texelSize;
            float depth = shadowMap.SampleLevel(shadowSampler, projCoords.xy + offset, 0).r;
            shadow += (projCoords.z <= depth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    float fadeStart = 0.85;
    float maxDist = max(projCoords.x, projCoords.y);
    float minDist = min(projCoords.x, projCoords.y);
    float edgeFade = smoothstep(0.0, 0.15, minDist) * (1.0 - smoothstep(fadeStart, 1.0, maxDist));
    shadow = lerp(1.0, shadow, edgeFade);

    return shadow;
}

float4 PSMain(PSInput input) : SV_Target {
    float4 texelColor = texture0.Sample(sampler0, input.texCoord);

    if (texelColor.a < 0.1)
        discard;

    // sRGB -> linear
    texelColor.rgb = pow(texelColor.rgb, 2.2);

    float3 tintedColor = texelColor.rgb * input.tint;

    float3 normal  = normalize(input.normal);
    float3 viewDir = normalize(viewPos - input.worldPos);

    // Sun lighting
    float sunDiff = max(dot(normal, sunDir), 0.0);

    float shadowFactor = 1.0;
    if (shadowEnabled) {
        shadowFactor = calcShadow(input.posLightSpace);
    }

    // Specular (Blinn-Phong)
    float3 halfDir = normalize(sunDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 64.0);
    spec *= (1.0 - input.foliageMask * 0.9);
    float3 specular = sunColor * spec * 0.3 * shadowFactor;

    // Sky light
    float minAmbient = 0.08;
    float skylightFactor = max(input.skyLight, minAmbient);

    // Directional + ambient
    float3 lighting = ambientColor * skylightFactor;
    lighting += sunColor * sunDiff * shadowFactor * skylightFactor;

    // Point lights
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i >= lightCount) break;

        PointLight light = lights[i];
        float3 lightDir = light.position - input.worldPos;
        float distance = length(lightDir);

        if (distance < light.radius) {
            lightDir = normalize(lightDir);
            float diff = max(dot(normal, lightDir), 0.0);
            float att = 1.0 - smoothstep(0.0, light.radius, distance);
            att = att * att;
            float3 pointContrib = light.color * light.intensity * diff * att;
            lighting += pointContrib;

            float3 plHalf = normalize(lightDir + viewDir);
            float plSpec = pow(max(dot(normal, plHalf), 0.0), 32.0);
            specular += light.color * light.intensity * plSpec * 0.15 * att;
        }
    }

    float ao = input.ao;
    float3 finalRGB = tintedColor * lighting * ao + specular * ao;

    // Distance fog
    float fogFactor = smoothstep(fogStart, fogEnd, input.dist);
    finalRGB = lerp(finalRGB, fogColor, fogFactor);

    return float4(finalRGB, texelColor.a) * colDiffuse;
}
