#version 330

// Vertex attributes (explicit locations matching GLMesh::Attrib)
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;
layout(location = 2) in vec2 vertexTexCoord2;  // .x = foliageMask, .y = ao (0..1)
layout(location = 3) in vec3 vertexNormal;
layout(location = 4) in vec4 vertexColor;      // RGBA: rgb = tint, a = skylight

out vec2 fragTexCoord;
out vec3 fragTint;
out float fragSkyLight;
out vec3 fragNormal;
out vec3 fragPos;
out float fragAO;
out float fragFoliageMask;
out vec4 fragPosLightSpace;
out float fragDist;             // distance from camera for fog

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightSpaceMatrix;  // Shadow map VP
uniform vec3 viewPos;

void main() {
    fragTexCoord = vertexTexCoord;
    fragTint = vertexColor.rgb;
    fragSkyLight = vertexColor.a; // Normalized 0..1 from 0..255

    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragAO = vertexTexCoord2.y;
    fragFoliageMask = vertexTexCoord2.x;

    // Shadow map space position
    fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);

    // Distance from camera (for fog)
    fragDist = length(fragPos - viewPos);

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
