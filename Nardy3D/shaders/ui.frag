// Text from the font atlases, and shapes drawn from their distance functions.

layout(binding = 8) uniform sampler2D uFont0;
layout(binding = 9) uniform sampler2D uFont1;
layout(binding = 10) uniform sampler2D uFont2;

in vec2 vUv;
in vec4 vColor;
flat in vec4 vShape; // x: kind; yzw depend on it

layout(location = 0) out vec4 fragColor;

float RoundedBox(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float Triangle(vec2 p, vec2 p0, vec2 p1, vec2 p2)
{
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    vec2 q0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 q1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 q2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(q0, q0), s * (v0.x * e0.y - v0.y * e0.x)), vec2(dot(q1, q1), s * (v1.x * e1.y - v1.y * e1.x))),
                 vec2(dot(q2, q2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

void main()
{
    int kind = int(vShape.x + 0.5);
    float alpha = 1.0;
    if (kind == 0)
        alpha = textureLod(uFont0, vUv, 0.0).r;
    else if (kind == 1)
        alpha = textureLod(uFont1, vUv, 0.0).r;
    else if (kind == 2)
        alpha = textureLod(uFont2, vUv, 0.0).r;
    else if (kind == 3) {
        // A rounded rectangle; uv is the position from its center, yz its half size, w the corner radius.
        alpha = Saturate(0.5 - RoundedBox(vUv, vShape.yz, vShape.w));
    }
    else if (kind == 4) {
        // A soft shadow, blurred by w.
        float d = RoundedBox(vUv, vShape.yz, vShape.w);
        alpha = 1.0 - smoothstep(-vShape.w, vShape.w, d);
        alpha *= alpha;
    }
    else if (kind == 5) {
        // The outline of a rounded rectangle.
        float d = RoundedBox(vUv, vShape.yz, vShape.w);
        alpha = Saturate(1.0 - abs(d + 0.75));
    }
    else if (kind == 6) {
        // A spinning arc: y is the radius, z the thickness, w the angle of its head.
        float r = length(vUv);
        float ring = Saturate(vShape.z * 0.5 + 0.5 - abs(r - vShape.y));
        float angle = mod(atan(vUv.y, vUv.x) - vShape.w, 6.2831853);
        alpha = ring * smoothstep(1.0, 5.5, angle) * (1.0 - smoothstep(5.6, 6.2, angle));
    }
    else if (kind == 7) {
        // A disc of radius y.
        alpha = Saturate(vShape.y + 0.5 - length(vUv));
    }
    else if (kind == 8) {
        // A triangle pointing along x, mirrored when w is negative; y and z are its half width and height.
        vec2 p = vec2(vUv.x * sign(vShape.w), vUv.y);
        alpha = Saturate(0.5 - Triangle(p, vec2(-vShape.y, -vShape.z), vec2(vShape.y, 0.0), vec2(-vShape.y, vShape.z)));
    }
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
