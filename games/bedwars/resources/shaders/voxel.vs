#version 330

// Raylib default vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;  // .x = foliageMask, .y = ao (0..1)
in vec3 vertexNormal;
in vec4 vertexColor;      // RGBA: rgb = tint, a = skylight

out vec2 fragTexCoord;
out vec3 fragTint;
out float fragSkyLight;
out vec3 fragNormal;
out vec3 fragPos;
out float fragAO;
out float fragFoliageMask;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

void main() {
    fragTexCoord = vertexTexCoord;
    fragTint = vertexColor.rgb;
    fragSkyLight = vertexColor.a; // Normalized 0..1 from 0..255

    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragAO = vertexTexCoord2.y;
    fragFoliageMask = vertexTexCoord2.x;

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
