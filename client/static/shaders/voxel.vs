#version 330

// Raylib default vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;  // .x = foliageMask, .y = ao (0..1)
in vec3 vertexNormal;
in vec4 vertexColor;      // RGBA tint (foliage/grass or white)

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out float fragAO;
out float fragFoliageMask;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragAO = vertexTexCoord2.y;
    fragFoliageMask = vertexTexCoord2.x;

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
