#version 330

in vec2 fragTexCoord;
in vec3 fragTint;
in float fragSkyLight;
in vec3 fragNormal;
in vec3 fragPos;
in float fragAO;
in float fragFoliageMask;

out vec4 finalColor;

uniform sampler2D texture0;  // Atlas texture
uniform vec4 colDiffuse;     // Material diffuse color (usually WHITE)

uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 ambientColor;

#define MAX_LIGHTS 32

struct PointLight {
    vec3 position;
    vec3 color;
    float radius;
    float intensity;
};

uniform int lightCount;
uniform PointLight lights[MAX_LIGHTS];
uniform vec3 viewPos;

void main() {
    // Sample texture from atlas
    vec4 texelColor = texture(texture0, fragTexCoord);

    // Discard fully transparent pixels (for leaves, etc.)
    if (texelColor.a < 0.1) {
        discard;
    }

    // Apply vertex color tint (foliage/grass recolor or white)
    vec3 tintedColor = texelColor.rgb * fragTint;

    vec3 normal = normalize(fragNormal);

    float sunDiff = max(dot(normal, sunDir), 0.0);
    
    float minAmbient = 0.15;
    float skylightFactor = max(fragSkyLight, minAmbient);
    
    vec3 lighting = (ambientColor + sunColor * sunDiff) * skylightFactor;

    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i >= lightCount) break;
        
        PointLight light = lights[i];
        vec3 lightDir = light.position - fragPos;
        float distance = length(lightDir);
        
        if (distance < light.radius) {
            lightDir = normalize(lightDir);
            float diff = max(dot(normal, lightDir), 0.0);
            
            float att = 1.0 - smoothstep(0.0, light.radius, distance);
            att = att * att; // Make it fall off nicer
            // BTW I love this line of code

            lighting += light.color * light.intensity * diff * att;
        }
    }

    // Apply Minecraft-style AO
    // AO ranges from ~0.2 (fully occluded corner) to 1.0 (no occlusion)
    float ao = fragAO;

    // Combine lighting with AO
    vec3 finalRGB = tintedColor * lighting * ao;

    finalColor = vec4(finalRGB, texelColor.a) * colDiffuse;
}
