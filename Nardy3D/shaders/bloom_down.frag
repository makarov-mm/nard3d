// Halves the image with a 13-tap filter; the first step weights bright samples down against flicker.

layout(binding = 0) uniform sampler2D uSource;

uniform vec2 uSourceTexel;
uniform int uFirst;

in vec2 vUv;

layout(location = 0) out vec4 fragColor;

vec3 Sample(vec2 offset) { return max(textureLod(uSource, vUv + offset * uSourceTexel, 0.0).rgb, vec3(0.0)); }

void main()
{
    vec3 a = Sample(vec2(-2, 2)), b = Sample(vec2(0, 2)), c = Sample(vec2(2, 2));
    vec3 d = Sample(vec2(-2, 0)), e = Sample(vec2(0, 0)), f = Sample(vec2(2, 0));
    vec3 g = Sample(vec2(-2, -2)), h = Sample(vec2(0, -2)), i = Sample(vec2(2, -2));
    vec3 j = Sample(vec2(-1, 1)), k = Sample(vec2(1, 1)), l = Sample(vec2(-1, -1)), m = Sample(vec2(1, -1));

    vec3 result;
    if (uFirst != 0) {
        // Each group of four counts less the brighter it is, so lone bright pixels do not flicker.
        vec3 groups[5] = vec3[]((j + k + l + m) * 0.25, (a + b + d + e) * 0.25, (b + c + e + f) * 0.25, (d + e + g + h) * 0.25, (e + f + h + i) * 0.25);
        float share[5] = float[](0.5, 0.125, 0.125, 0.125, 0.125);
        vec3 sum = vec3(0.0);
        float weights = 0.0;
        for (int n = 0; n < 5; ++n) {
            float weight = share[n] / (1.0 + Luminance(groups[n]));
            sum += groups[n] * weight;
            weights += weight;
        }
        result = sum / weights;
    }
    else {
        result = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    }
    fragColor = vec4(result, 1.0);
}
