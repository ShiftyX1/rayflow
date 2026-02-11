#version 330

// Shadow depth pass — fragment shader
// Discards transparent pixels (leaves, vegetation) so they don't cast shadow.

in vec2 fragTexCoord;

uniform sampler2D texture0;  // Atlas texture for alpha test

void main() {
    float alpha = texture(texture0, fragTexCoord).a;
    if (alpha < 0.5) {
        discard;
    }
    // gl_FragDepth is written automatically
}
