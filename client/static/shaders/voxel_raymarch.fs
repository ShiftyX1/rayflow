#version 330

in vec2 fragTexCoord;
in vec2 fragTexCoord2;
in vec4 fragColor;
in vec3 fragPosWS;
in vec3 fragNormalWS;

out vec4 finalColor;

uniform sampler2D texture0;
uniform sampler2D u_occTex;

uniform float u_enabled;

uniform vec3 u_sunDirWS;
uniform vec3 u_sunColor;
uniform vec3 u_ambientColor;

// MV-2: temperature-driven foliage/grass color.
uniform vec3 u_foliageColor;

// Minecraft-like gamma shaping for light channels. Values > 1.0 lift dark regions.
uniform float u_lightGamma;

uniform vec3 u_volumeOriginWS;
uniform vec3 u_volumeSize;
uniform vec2 u_occInvSize;

uniform float u_stepSize;
uniform int u_maxSteps;

const int MAX_STEPS = 64;

float apply_gamma01(float x, float gamma) {
    x = clamp(x, 0.0, 1.0);
    if (gamma <= 0.0) return x;
    // Lift dark values when gamma > 1.
    return pow(x, 1.0 / gamma);
}

float sample_occ(ivec3 v) {
    if (v.x < 0 || v.y < 0 || v.z < 0) return 0.0;
    if (v.x >= int(u_volumeSize.x) || v.y >= int(u_volumeSize.y) || v.z >= int(u_volumeSize.z)) return 0.0;

    int row = v.z * int(u_volumeSize.y) + v.y;

    float u = (float(v.x) + 0.5) * u_occInvSize.x;
    float vv = (float(row) + 0.5) * u_occInvSize.y;

    return texture(u_occTex, vec2(u, vv)).r;
}

float compute_shadow(vec3 p_ws, vec3 n_ws) {
    vec3 start = p_ws + n_ws * 0.05;

    float t = 0.0;
    for (int i = 0; i < MAX_STEPS; ++i) {
        if (i >= u_maxSteps) break;

        vec3 p = start + u_sunDirWS * t;
        vec3 v = p - u_volumeOriginWS;
        ivec3 vi = ivec3(floor(v));

        if (sample_occ(vi) > 0.5) {
            return 0.0;
        }

        t += u_stepSize;
    }

    return 1.0;
}

float volume_shadow_fade(vec3 p_ws) {
    // Fade shadows out near the edge of the occupancy volume to avoid visible hard boundaries.
    vec3 v = p_ws - u_volumeOriginWS;

    // Outside the volume: no raymarched shadows.
    if (v.x < 0.0 || v.y < 0.0 || v.z < 0.0) return 0.0;
    if (v.x > u_volumeSize.x || v.y > u_volumeSize.y || v.z > u_volumeSize.z) return 0.0;

    // Distance to each boundary in voxels.
    float dx = min(v.x, u_volumeSize.x - v.x);
    float dy = min(v.y, u_volumeSize.y - v.y);
    float dz = min(v.z, u_volumeSize.z - v.z);

    float d = min(dx, min(dy, dz));

    // 0 at boundary, 1 inside. 4-voxel feather is a good default.
    return smoothstep(0.0, 4.0, d);
}

void main() {
    vec4 texel = texture(texture0, fragTexCoord);
    vec4 albedo = texel;

    // 0 -> normal shading, 1 -> full foliage recolor
    float foliageMask = clamp(fragTexCoord2.x, 0.0, 1.0);
    float lum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
    vec3 recolor = u_foliageColor * lum;
    vec3 baseRgb = mix(albedo.rgb, recolor, foliageMask);
    float baseA = albedo.a;

    // Packed per-vertex lighting:
    // - fragColor.r: skylight [0..1]
    // - fragColor.g: blocklight [0..1]
    // - fragTexCoord2.y: ambient occlusion [0..1]
    float skyL = apply_gamma01(fragColor.r, u_lightGamma);
    float blkL = apply_gamma01(fragColor.g, u_lightGamma);
    float ao = clamp(fragTexCoord2.y, 0.0, 1.0);

    if (u_enabled < 0.5) {
        // When disabled: still apply AO + light channels (so lighting stays consistent).
        float shade = ao * max(skyL, blkL);
        finalColor = vec4(baseRgb * shade, baseA);
        return;
    }

    vec3 n = normalize(fragNormalWS);
    float ndl = max(dot(n, normalize(u_sunDirWS)), 0.0);

    float shadow = compute_shadow(fragPosWS, n);
    shadow = mix(1.0, shadow, volume_shadow_fade(fragPosWS));

    // Compose lighting in a Minecraft-like way:
    // - AO darkens only ambient + blocklight (so it doesn't look like a second sun shadow)
    // - Raymarched shadow affects only the sun direct term
    vec3 directSun = (u_sunColor * ndl * shadow) * skyL;
    vec3 ambientSky = (u_ambientColor * skyL) * ao;
    vec3 blockTerm = (vec3(1.0) * blkL) * ao;

    vec3 lit = ambientSky + blockTerm + directSun;
    finalColor = vec4(baseRgb * lit, baseA);
}
