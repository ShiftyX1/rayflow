#version 330

// Tone mapping — fragment shader
//
// Lottes 2016 tone mapping (used by Complementary Reimagined)
// Reference: "Advanced Techniques and Optimization of HDR Color Pipelines"
//            Timothy Lottes, GDC 2016
//
// Also applies sRGB gamma correction and optional vignette.

in vec2 texCoord;

out vec4 finalColor;

uniform sampler2D hdrBuffer;
uniform float exposure;

// Lottes tone mapping curve
// color_out = pow(color, a) / (pow(color, a*d) * b + c)
// Derived from: hdrMax=8.0, midIn=0.25, midOut=0.25

vec3 lottesTonemap(vec3 color) {
    const float a = 1.4;  // contrast
    const float d = 0.977;
    const float hdrMax = 8.0;
    const float midIn = 0.18;
    const float midOut = 0.18;

    // Compute b and c from constraints
    float b = (-pow(midIn, a) + pow(hdrMax, a) * midOut)
            / ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    float c = (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut)
            / ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    return pow(color, vec3(a)) / (pow(color, vec3(a * d)) * b + c);
}

// sRGB transfer function (more accurate than simple pow 1/2.2)
vec3 linearToSRGB(vec3 color) {
    vec3 lo = color * 12.92;
    vec3 hi = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), color));
}

void main() {
    vec3 hdrColor = texture(hdrBuffer, texCoord).rgb;

    // Apply exposure
    hdrColor *= exposure;

    // Tone mapping
    vec3 mapped = lottesTonemap(hdrColor);

    // Clamp before gamma (safety)
    mapped = clamp(mapped, 0.0, 1.0);

    // Vignette (subtle)
    vec2 uv = texCoord * 2.0 - 1.0;
    float vignette = 1.0 - dot(uv, uv) * 0.15;
    mapped *= vignette;

    // Linear → sRGB
    vec3 srgb = linearToSRGB(mapped);

    finalColor = vec4(srgb, 1.0);
}
