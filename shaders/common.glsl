// Shared by the scene shaders: the per-frame data, noise, and physically based shading.

const float PI = 3.14159265359;

layout(std140, binding = 0) uniform FrameData {
    mat4 uViewProjection;
    mat4 uInverseViewProjection;
    mat4 uLightViewProjection;
    vec4 uCameraPosition;   // xyz, w: time in seconds
    vec4 uLightDirection;   // toward the light; w: world size of a shadow map texel
    vec4 uLightColor;       // radiance; w: tangent of the light's angular radius, for soft shadows
    vec4 uShadowParams;     // x: depth range of the shadow map, y: depth bias, z: where the fog begins
    vec4 uViewport;         // width, height, 1 / width, 1 / height
    vec4 uEnvironment;      // x: diffuse light, y: reflected light, z: highest mip of the prefiltered map, w: background
    vec4 uAccent;           // highlights
    vec4 uFog;              // rgb, w: density
};

// Per pass: where the shading looks from.
layout(std140, binding = 1) uniform PassData {
    vec4 uEye;              // xyz; w: 1 to leave out the shadows
};

float Saturate(float x) { return clamp(x, 0.0, 1.0); }
vec2 Saturate(vec2 x) { return clamp(x, 0.0, 1.0); }
vec3 Saturate(vec3 x) { return clamp(x, 0.0, 1.0); }
float Luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// Display -------------------------------------------------------------------------------------------

vec3 RrtAndOdtFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

// The ACES filmic curve, after Stephen Hill's fit.
vec3 Aces(vec3 color)
{
    const mat3 inputMatrix = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
    const mat3 outputMatrix = mat3(1.60475, -0.10208, -0.00327, -0.53108, 1.10813, -0.07276, -0.07367, -0.00605, 1.07602);
    return Saturate(outputMatrix * RrtAndOdtFit(inputMatrix * color));
}

vec3 EncodeSrgb(vec3 c)
{
    return mix(1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, 12.92 * c, step(c, vec3(0.0031308)));
}

// Noise -------------------------------------------------------------------------------------------

float Hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float Hash13(vec3 p3)
{
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}

float Noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(Hash12(i), Hash12(i + vec2(1, 0)), u.x), mix(Hash12(i + vec2(0, 1)), Hash12(i + vec2(1, 1)), u.x), u.y);
}

float Noise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float a = mix(mix(Hash13(i), Hash13(i + vec3(1, 0, 0)), u.x), mix(Hash13(i + vec3(0, 1, 0)), Hash13(i + vec3(1, 1, 0)), u.x), u.y);
    float b = mix(mix(Hash13(i + vec3(0, 0, 1)), Hash13(i + vec3(1, 0, 1)), u.x), mix(Hash13(i + vec3(0, 1, 1)), Hash13(i + vec3(1, 1, 1)), u.x), u.y);
    return mix(a, b, u.z);
}

float Fbm(vec2 p)
{
    float sum = 0.0, amplitude = 0.5;
    for (int i = 0; i < 5; ++i) {
        sum += amplitude * Noise(p);
        p = mat2(1.6, 1.2, -1.2, 1.6) * p + 7.13;
        amplitude *= 0.5;
    }
    return sum;
}

float Fbm(vec3 p)
{
    float sum = 0.0, amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        sum += amplitude * Noise(p);
        p = p * 2.03 + vec3(5.1, 1.7, 9.2);
        amplitude *= 0.5;
    }
    return sum;
}

// Anti-aliasing ----------------------------------------------------------------------------------

// The share of a pixel that a line covers: the pixel is `pixel` wide with its middle `d` from the line's
// middle, and the line reaches `halfWidth` to either side. Thin lines fade instead of breaking up.
float LineCoverage(float d, float halfWidth, float pixel)
{
    return Saturate((min(d + 0.5 * pixel, halfWidth) - max(d - 0.5 * pixel, -halfWidth)) / pixel);
}

// Per-pixel noise for dithering and for rotating sample patterns.
float InterleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}
