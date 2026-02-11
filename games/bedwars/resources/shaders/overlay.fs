#version 330

in vec2 fragTexCoord;

out vec4 finalColor;

uniform sampler2D tex;
uniform vec4 tintColor;

void main() {
    vec4 texel = texture(tex, fragTexCoord);
    if (texel.a < 0.01) discard;
    finalColor = texel * tintColor;
}
