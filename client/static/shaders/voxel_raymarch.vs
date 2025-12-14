#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosWS;
out vec3 fragNormalWS;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    vec4 worldPos = matModel * vec4(vertexPosition, 1.0);
    fragPosWS = worldPos.xyz;
    fragNormalWS = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
