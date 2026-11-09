// The checkers, turned of wood, stone or bone with a raised rim, a carved groove and a rosette in the middle; and the
// dice, ivory or plastic with rounded edges and sunken pips.

in vec3 vWorld;
in vec3 vNormal;
in vec3 vLocal;
in vec2 vUv;
flat in vec4 vParams;
flat in vec4 vExtra;

uniform vec4 uCheckerColor[2]; // White's and Black's: rgb, roughness
uniform vec4 uCheckerStyle[2]; // clear coat, pattern (0 plain, 1 wood, 2 stone, 3 bone), seed, strength of the ornament
uniform vec4 uDieColor;        // rgb, roughness
uniform vec4 uPipColor;        // rgb, 0
uniform float uCheckerTop;     // the checker's height over its radius

layout(location = 0) out vec4 fragColor;

// Rings round the middle of a turned piece, and fibres along them.
vec3 TurnedWood(vec3 color, float r, float angle, float seed, float footprint)
{
    float t = r * 9.0 + 0.6 * Noise(vec2(angle * 3.0, seed * 7.0)) + 0.4 * Fbm(vec2(r * 4.0, angle * 0.8 + seed));
    float ring = abs(fract(t) - 0.5) * 2.0;
    float late = mix(smoothstep(0.55, 0.95, ring), 0.25, smoothstep(0.02, 0.08, footprint));
    float fibres = mix(Noise(vec2(r * 300.0, angle * 40.0)), 0.5, smoothstep(0.002, 0.008, footprint));
    return color * (0.86 + 0.22 * late) * (0.92 + 0.14 * fibres);
}

vec3 Stone(vec3 color, vec3 p, float seed)
{
    float warp = Fbm(p.xz * 2.2 + seed * 13.0) * 3.0;
    float vein = pow(1.0 - abs(sin((p.x * 2.0 + p.z * 1.3 + p.y * 3.0 + warp) * 3.0)), 18.0);
    vec3 c = color * (0.88 + 0.24 * Fbm(p.xz * 3.5 + seed * 3.0));
    float light = dot(color, vec3(0.333));
    return mix(c, light > 0.3 ? color * 0.55 : color + vec3(0.18), vein * 0.6);
}

vec3 Bone(vec3 color, vec3 p, float seed)
{
    float grain = Noise(vec2(p.x * 3.0 + seed, p.z * 40.0)) * 0.5 + 0.5 * Fbm(p.xz * 6.0 + seed);
    return color * (0.94 + 0.1 * grain);
}

// The pips of a face, as ink from 0 to 1, in the face's own coordinates from -1 to 1.
float Pips(int value, vec2 uv, float pixel)
{
    const float r = 0.19;
    float ink = 0.0;
    vec2 spots[6];
    int count = 0;
    if (value == 1) { spots[0] = vec2(0.0); count = 1; }
    else if (value == 2) { spots[0] = vec2(-0.48, -0.48); spots[1] = vec2(0.48, 0.48); count = 2; }
    else if (value == 3) { spots[0] = vec2(-0.5, -0.5); spots[1] = vec2(0.0); spots[2] = vec2(0.5, 0.5); count = 3; }
    else if (value == 4) { spots[0] = vec2(-0.48, -0.48); spots[1] = vec2(0.48, -0.48); spots[2] = vec2(-0.48, 0.48); spots[3] = vec2(0.48, 0.48); count = 4; }
    else if (value == 5) { spots[0] = vec2(-0.5, -0.5); spots[1] = vec2(0.5, -0.5); spots[2] = vec2(-0.5, 0.5); spots[3] = vec2(0.5, 0.5); spots[4] = vec2(0.0); count = 5; }
    else { spots[0] = vec2(-0.48, -0.55); spots[1] = vec2(-0.48, 0.0); spots[2] = vec2(-0.48, 0.55); spots[3] = vec2(0.48, -0.55); spots[4] = vec2(0.48, 0.0); spots[5] = vec2(0.48, 0.55); count = 6; }
    for (int i = 0; i < count; ++i)
        ink = max(ink, 1.0 - smoothstep(r - pixel, r + pixel, length(uv - spots[i])));
    return ink;
}

void main()
{
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing)
        N = -N;
    vec3 V = normalize(uEye.xyz - vWorld);
    float spread = NormalSpread(N);
    int kind = int(vParams.x + 0.5);
    int side = int(vParams.y + 0.5);
    Material m = DefaultMaterial(vec3(0.5), 0.4);

    if (kind == 0) {
        vec4 color = uCheckerColor[side];
        vec4 style = uCheckerStyle[side];
        vec3 p = vLocal;
        float r = length(p.xz);
        float angle = atan(p.z, p.x);
        float footprint = length(fwidth(p.xz));
        int pattern = int(style.y + 0.5);
        vec3 albedo = color.rgb;
        if (pattern == 1)
            albedo = TurnedWood(color.rgb, r, angle, style.z, footprint);
        else if (pattern == 2)
            albedo = Stone(color.rgb, p * 1.6, style.z);
        else if (pattern == 3)
            albedo = Bone(color.rgb, p, style.z);
        else
            albedo *= 0.96 + 0.08 * Noise(p.xz * 30.0 + style.z);

        // The top face: the carved groove and a rosette of eight petals in the middle, darker in their hollows.
        float top = smoothstep(uCheckerTop - 0.14, uCheckerTop - 0.07, p.y) * step(0.0, N.y);
        float pixel = max(footprint, 1e-4);
        float groove = 1.0 - smoothstep(0.03, 0.05 + pixel, abs(r - 0.46));
        float petals = 0.5 + 0.5 * cos(angle * 8.0);
        float rosette = (1.0 - smoothstep(0.24 - pixel, 0.26 + pixel, r * (1.0 + 0.45 * (1.0 - petals)))) * smoothstep(0.03, 0.05, r);
        float hub = 1.0 - smoothstep(0.05 - pixel, 0.05 + pixel, r);
        float carving = top * max(groove * 0.8, (rosette * 0.75 + hub) * style.w);
        albedo *= 1.0 - 0.45 * carving;
        m.albedo = albedo;
        m.roughness = mix(color.a, min(1.0, color.a + 0.25), carving);
        m.clearcoat = style.x * (1.0 - carving * 0.6);
        m.clearcoatRoughness = 0.08;
        m.occlusion = mix(1.0, 0.55, carving) * mix(0.75, 1.0, smoothstep(0.0, 0.08, p.y));
    }
    else {
        // The face is the one the point lies furthest along; its numbers as on a die: 1 on +y, 2 on +z, 3 on +x.
        vec3 p = vLocal;
        vec3 a = abs(p);
        int value;
        vec2 uv;
        if (a.y >= a.x && a.y >= a.z) { value = p.y > 0.0 ? 1 : 6; uv = p.xz; }
        else if (a.x >= a.z) { value = p.x > 0.0 ? 3 : 4; uv = p.zy; }
        else { value = p.z > 0.0 ? 2 : 5; uv = p.xy; }
        float pixel = max(length(fwidth(uv)), 1e-4) * 0.8;
        float ink = Pips(value, uv, pixel);
        // A pip is a hollow: dark ink and a little shade round it.
        vec3 albedo = uDieColor.rgb * (0.95 + 0.06 * Noise(p.xy * 20.0 + p.z * 7.0));
        albedo = mix(albedo, uPipColor.rgb, ink);
        float dim = vExtra.x;
        albedo = mix(albedo, vec3(dot(albedo, vec3(0.3, 0.5, 0.2))) * 0.45, dim * 0.7);
        m.albedo = albedo;
        m.roughness = mix(uDieColor.a, 0.6, ink);
        m.clearcoat = 0.6 * (1.0 - ink);
        m.clearcoatRoughness = 0.06;
        m.subsurface = 0.25 * (1.0 - ink);
        m.occlusion = mix(1.0, 0.6, ink);
    }

    m.roughness = WidenRoughness(m.roughness, spread);
    m.clearcoatRoughness = WidenRoughness(m.clearcoatRoughness, spread);
    float shadow = uEye.w > 0.5 ? 1.0 : Shadow(vWorld, N);
    vec3 color = Shade(m, vWorld, N, V, shadow);
    // A glow at the rim of what may be moved or is chosen.
    float highlight = vParams.w;
    if (highlight > 0.0) {
        float rim = pow(1.0 - Saturate(dot(N, V)), 2.0);
        color += uAccent.rgb * highlight * (0.12 + 1.2 * rim);
    }
    fragColor = vec4(ApplyFog(color, vWorld), 1.0);
}
