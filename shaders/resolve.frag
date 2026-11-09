// Averages the samples of each pixel, weighting bright ones down so highlights keep smooth edges.

layout(binding = 0) uniform sampler2DMS uSource;

uniform int uSamples;

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 sum = vec3(0.0);
    float weights = 0.0;
    for (int i = 0; i < uSamples; ++i) {
        vec3 color = max(texelFetch(uSource, pixel, i).rgb, vec3(0.0));
        float weight = 1.0 / (1.0 + Luminance(color));
        sum += color * weight;
        weights += weight;
    }
    fragColor = vec4(sum / weights, 1.0);
}
