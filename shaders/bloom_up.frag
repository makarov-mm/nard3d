// Doubles the image with a tent filter; added onto the next larger level.

layout(binding = 0) uniform sampler2D uSource;

uniform vec2 uSourceTexel;

in vec2 vUv;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 t = uSourceTexel;
    vec3 sum = textureLod(uSource, vUv, 0.0).rgb * 4.0;
    sum += (textureLod(uSource, vUv + vec2(-t.x, 0.0), 0.0).rgb + textureLod(uSource, vUv + vec2(t.x, 0.0), 0.0).rgb +
            textureLod(uSource, vUv + vec2(0.0, -t.y), 0.0).rgb + textureLod(uSource, vUv + vec2(0.0, t.y), 0.0).rgb) * 2.0;
    sum += textureLod(uSource, vUv + vec2(-t.x, -t.y), 0.0).rgb + textureLod(uSource, vUv + vec2(t.x, -t.y), 0.0).rgb +
           textureLod(uSource, vUv + vec2(-t.x, t.y), 0.0).rgb + textureLod(uSource, vUv + vec2(t.x, t.y), 0.0).rgb;
    fragColor = vec4(sum / 16.0, 1.0);
}
