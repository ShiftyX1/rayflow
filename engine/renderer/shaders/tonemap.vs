#version 330

// Tone mapping — vertex shader (fullscreen triangle)

layout(location = 0) in vec3 vertexPosition;

out vec2 texCoord;

void main() {
    gl_Position = vec4(vertexPosition, 1.0);
    // Map [-1,1] NDC to [0,1] UV
    texCoord = vertexPosition.xy * 0.5 + 0.5;
}
