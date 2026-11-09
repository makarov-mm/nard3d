// The final image: bloom, frosted glass behind the panels, exposure, filmic tone mapping, vignette, dither.

layout(binding = 0) uniform sampler2D uScene;
layout(binding = 1) uniform sampler2D uBloom;
layout(binding = 2) uniform sampler2D uBlur;

uniform float uExposure;
uniform float uBloomStrength;
uniform float uVignette;
uniform vec4 uPanels[4];     // center x, center y, half width, half height, in pixels from the top left
uniform vec4 uPanelStyle[4]; // x: corner radius, y: opacity
uniform int uPanelCount;

in vec2 vUv;

layout(location = 0) out vec4 fragColor;

float RoundedBox(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    vec3 color = texture(uScene, vUv).rgb;
    vec3 bloom = textureLod(uBloom, vUv, 0.0).rgb / 6.0;
    color = mix(color, bloom, uBloomStrength);

    vec2 pixel = vec2(gl_FragCoord.x, uViewport.y - gl_FragCoord.y);
    for (int i = 0; i < uPanelCount; ++i) {
        float d = RoundedBox(pixel - uPanels[i].xy, uPanels[i].zw, uPanelStyle[i].x);
        float inside = 1.0 - smoothstep(-1.0, 1.0, d);
        vec3 frosted = textureLod(uBlur, vUv, 0.0).rgb / 4.0;
        color = mix(color, frosted * 0.45, inside * uPanelStyle[i].y);
    }

    color = Aces(color * uExposure);

    vec2 centered = vUv - 0.5;
    centered.x *= uViewport.x * uViewport.w;
    color *= 1.0 - uVignette * smoothstep(0.25, 1.05, length(centered));

    color = EncodeSrgb(color);
    color += (InterleavedGradientNoise(gl_FragCoord.xy) - 0.5) / 255.0;
    fragColor = vec4(color, 1.0);
}
