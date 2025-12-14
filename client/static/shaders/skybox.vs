#version 330

in vec3 vertexPosition;

out vec3 fragDir;

uniform mat4 mvp;

void main() {
    fragDir = vertexPosition;

    // Keep the skybox at the far plane.
    vec4 pos = mvp * vec4(vertexPosition, 1.0);
    gl_Position = pos.xyww;
}
