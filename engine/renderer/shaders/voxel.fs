#version 330

// ============================================================================
// Voxel fragment shader — Complementary-inspired lighting pipeline
//
// Features:
//   - Directional sun light with shadow mapping (PCF soft shadows)
//   - Blinn-Phong specular highlights
//   - Point lights with attenuation
//   - Minecraft-style per-vertex ambient occlusion
//   - Sky light + block light dual lightmap
//   - Distance fog (hides chunk loading boundary)
//   - HDR output (tone mapped in post-process)
// ============================================================================

in vec2 fragTexCoord;
in vec3 fragTint;
in float fragSkyLight;
in vec3 fragNormal;
in vec3 fragPos;
in float fragAO;
in float fragFoliageMask;
in vec4 fragPosLightSpace;
in float fragDist;

out vec4 finalColor;

uniform sampler2D texture0;        // Atlas texture
uniform sampler2DShadow shadowMap; // Shadow depth map (hardware comparison)
uniform vec4 colDiffuse;           // Material diffuse color (usually WHITE)

// Sun / ambient
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 ambientColor;

// Fog
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

// Shadow
uniform bool shadowEnabled;

// Camera
uniform vec3 viewPos;

// Point lights
#define MAX_LIGHTS 32

struct PointLight {
    vec3 position;
    vec3 color;
    float radius;
    float intensity;
};

uniform int lightCount;
uniform PointLight lights[MAX_LIGHTS];

// =============================================================================
// Shadow calculation (PCF 3x3)
// =============================================================================

float calcShadow(vec4 posLightSpace) {
    // Perspective divide
    vec3 projCoords = posLightSpace.xyz / posLightSpace.w;

    // Transform from [-1,1] to [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // Outside shadow map — no shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    // PCF 3x3 soft shadows using hardware comparison (sampler2DShadow)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            // texture() on sampler2DShadow returns comparison result [0,1]
            shadow += texture(shadowMap, vec3(projCoords.xy + offset, projCoords.z));
        }
    }
    shadow /= 9.0;

    // Fade shadow at distance (avoid hard cutoff at shadow map boundary)
    float fadeStart = 0.85;
    float maxDist = max(projCoords.x, projCoords.y);
    float minDist = min(projCoords.x, projCoords.y);
    float edgeFade = smoothstep(0.0, 0.15, minDist) * (1.0 - smoothstep(fadeStart, 1.0, maxDist));
    shadow = mix(1.0, shadow, edgeFade);

    return shadow;
}

// =============================================================================
// Main
// =============================================================================

void main() {
    // Sample texture from atlas
    vec4 texelColor = texture(texture0, fragTexCoord);

    // Discard fully transparent pixels (for leaves, etc.)
    if (texelColor.a < 0.1) {
        discard;
    }

    // sRGB → linear conversion.
    // Atlas textures are sRGB-encoded PNGs loaded as GL_RGBA8 (no HW decode).
    // The HDR pipeline expects linear-space inputs, so we degamma here.
    texelColor.rgb = pow(texelColor.rgb, vec3(2.2));

    // Apply vertex color tint (foliage/grass recolor or white)
    vec3 tintedColor = texelColor.rgb * fragTint;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPos);

    // ---- Directional sun lighting ----
    float sunDiff = max(dot(normal, sunDir), 0.0);

    // Shadow
    float shadowFactor = 1.0;
    if (shadowEnabled) {
        shadowFactor = calcShadow(fragPosLightSpace);
    }

    // Specular (Blinn-Phong)
    vec3 halfDir = normalize(sunDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 64.0);
    // Reduce specular on grass/foliage (they're rough)
    spec *= (1.0 - fragFoliageMask * 0.9);
    vec3 specular = sunColor * spec * 0.3 * shadowFactor;

    // ---- Sky light factor ----
    float minAmbient = 0.08;
    float skylightFactor = max(fragSkyLight, minAmbient);

    // ---- Combine directional + ambient ----
    vec3 lighting = ambientColor * skylightFactor;
    lighting += sunColor * sunDiff * shadowFactor * skylightFactor;

    // ---- Point lights ----
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i >= lightCount) break;

        PointLight light = lights[i];
        vec3 lightDir = light.position - fragPos;
        float distance = length(lightDir);

        if (distance < light.radius) {
            lightDir = normalize(lightDir);
            float diff = max(dot(normal, lightDir), 0.0);

            float att = 1.0 - smoothstep(0.0, light.radius, distance);
            att = att * att; // Quadratic falloff

            vec3 pointContrib = light.color * light.intensity * diff * att;
            lighting += pointContrib;

            // Point light specular
            vec3 plHalf = normalize(lightDir + viewDir);
            float plSpec = pow(max(dot(normal, plHalf), 0.0), 32.0);
            specular += light.color * light.intensity * plSpec * 0.15 * att;
        }
    }

    // ---- Ambient occlusion ----
    float ao = fragAO;

    // ---- Combine final color (HDR) ----
    vec3 finalRGB = tintedColor * lighting * ao + specular * ao;

    // ---- Distance fog ----
    float fogFactor = smoothstep(fogStart, fogEnd, fragDist);
    finalRGB = mix(finalRGB, fogColor, fogFactor);

    finalColor = vec4(finalRGB, texelColor.a) * colDiffuse;
}
