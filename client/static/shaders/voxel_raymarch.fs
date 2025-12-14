#version 330

in vec2 fragTexCoord;
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

uniform vec3 u_volumeOriginWS;
uniform vec3 u_volumeSize;
uniform vec2 u_occInvSize;

uniform float u_stepSize;
uniform int u_maxSteps;

const int MAX_STEPS = 64;

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

void main() {
    vec4 albedo = texture(texture0, fragTexCoord) * fragColor;

    if (u_enabled < 0.5) {
        finalColor = albedo;
        return;
    }

    vec3 n = normalize(fragNormalWS);
    float ndl = max(dot(n, normalize(u_sunDirWS)), 0.0);

    float shadow = compute_shadow(fragPosWS, n);

    vec3 lit = u_ambientColor + (u_sunColor * ndl * shadow);
    finalColor = vec4(albedo.rgb * lit, albedo.a);
}
