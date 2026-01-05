#version 330

in vec3 fragDir;

out vec4 finalColor;

uniform samplerCube environmentMap;

void main() {
    vec3 dir = normalize(fragDir);
    vec3 rgb = texture(environmentMap, dir).rgb;
    finalColor = vec4(rgb, 1.0);
}
