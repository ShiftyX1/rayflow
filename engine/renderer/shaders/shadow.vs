#version 330

// Shadow depth pass — vertex shader
// Renders scene geometry from light's perspective for shadow mapping.

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;
layout(location = 4) in vec4 vertexColor;       // For alpha-test on foliage

out vec2 fragTexCoord;

uniform mat4 lightSpaceMatrix;

void main() {
    fragTexCoord = vertexTexCoord;
    gl_Position = lightSpaceMatrix * vec4(vertexPosition, 1.0);
}
