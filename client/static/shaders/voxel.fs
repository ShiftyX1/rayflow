#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in float fragAO;
in float fragFoliageMask;

out vec4 finalColor;

uniform sampler2D texture0;  // Atlas texture
uniform vec4 colDiffuse;     // Raylib diffuse color (usually WHITE)

void main() {
    // Sample texture from atlas
    vec4 texelColor = texture(texture0, fragTexCoord);

    // Discard fully transparent pixels (for leaves, etc.)
    if (texelColor.a < 0.1) {
        discard;
    }

    // Apply vertex color tint (foliage/grass recolor or white)
    // Note: fragColor is already normalized [0,1] by OpenGL
    vec3 tintedColor = texelColor.rgb * fragColor.rgb;

    // Simple directional light for depth perception
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    float NdotL = max(dot(fragNormal, lightDir), 0.0);
    float ambient = 0.6;
    float diffuse = 0.4;
    float lighting = ambient + diffuse * NdotL;

    // Apply Minecraft-style AO
    // AO ranges from ~0.2 (fully occluded corner) to 1.0 (no occlusion)
    float ao = fragAO;

    // Combine lighting with AO
    vec3 finalRGB = tintedColor * lighting * ao;

    finalColor = vec4(finalRGB, texelColor.a) * colDiffuse;
}
