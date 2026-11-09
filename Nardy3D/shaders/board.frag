// The board: its two fields with the points inlaid, the rims and the middle bar of polished wood, the felt of the
// tray, and the table under it; and the marks of the game drawn on the points: where a checker may go, which may
// move, the last move and the hint.

in vec3 vWorld;
in vec3 vNormal;
in vec2 vUv;

uniform int uKind;           // 0 the fields, 1 the wood, 2 the felt, 3 the table
uniform vec4 uLayout;        // a point's width and length, half the field's depth, half the bar's width
uniform vec3 uFieldColor;
uniform vec4 uFieldStyle;    // roughness, pattern (0 wood, 1 felt, 2 leather), seed, clear coat
uniform vec3 uPointColor[2];
uniform vec4 uPointStyle;    // roughness, half the width of the inlaid line, metallic of the line, 0
uniform vec3 uLineColor;
uniform vec3 uWoodColor;
uniform vec4 uWoodStyle;     // roughness, clear coat, pattern (0 wood, 1 lacquer), seed
uniform vec3 uFeltColor;
uniform vec3 uTableColor;
uniform vec4 uTableStyle;    // roughness, pattern (0 wood, 1 cloth, 2 stone), seed, 0
uniform vec4 uPoints[24];    // as White counts them: x a place to move to, y a checker that may move, z the last move, w the hint
uniform vec4 uTrays;         // x White's tray as a place to move to, y Black's; z the hint into White's (above 0) or Black's
uniform vec3 uHintColor;

layout(location = 0) out vec4 fragColor;

// Wood with its grain along x: growth lines of uneven spacing and strength, fine fibres and a soft figure.
// Detail finer than the footprint, the size of a pixel in p, fades to its average instead of flickering.
vec3 Wood(vec2 p, vec3 color, float seed, float footprint)
{
    p += vec2(seed * 31.7, seed * 17.3);
    float t = p.y * 7.0 + 2.5 * Fbm(vec2(p.y * 0.8, seed * 5.0)) + 0.8 * Fbm(vec2(p.x * 0.3, p.y * 1.2)) + 0.2 * sin(p.x * 1.1 + seed * 6.0);
    float ring = floor(t);
    float phase = t - ring;
    float width = 0.2 + 0.3 * Hash12(vec2(ring, seed));
    float late = smoothstep(0.0, 0.08, phase) * (1.0 - smoothstep(width, width + 0.25, phase));
    late *= 0.35 + 0.9 * Hash12(vec2(ring, seed + 7.0));
    late = mix(late, 0.3, smoothstep(0.04, 0.12, footprint));
    float fibers = 0.5 * Noise(vec2(p.x * 3.0, p.y * 260.0)) + 0.5 * Noise(vec2(p.x * 7.0, p.y * 600.0));
    fibers = mix(fibers, 0.5, smoothstep(0.001, 0.004, footprint));
    float figure = Noise(p * vec2(0.8, 2.2) + seed * 9.0);
    return color * (1.0 - 0.2 * late) * (0.9 + 0.16 * fibers) * (0.9 + 0.2 * figure);
}

// Polished stone: a soft mottle and a fine speckle of grains, the speckle fading where it is finer than a pixel.
vec3 Granite(vec2 p, vec3 color, float seed, float footprint)
{
    p += seed * 10.0;
    float mottle = Fbm(p * 1.6);
    float speckle = mix(Noise(p * 300.0), 0.5, smoothstep(0.001, 0.004, footprint));
    float flecks = smoothstep(0.66, 0.72, Noise(p * 140.0 + 3.0)) * (1.0 - smoothstep(0.002, 0.006, footprint));
    vec3 c = color * (0.9 + 0.18 * mottle) * (0.93 + 0.14 * speckle);
    return mix(c, color * 1.9, flecks * 0.18);
}

// Felt: fine fibres and a soft mottle, fading to their average where they are finer than a pixel.
vec3 Felt(vec3 color, vec2 p, float footprint)
{
    float fibres = 0.5 * Noise(p * vec2(900.0, 300.0)) + 0.5 * Noise(p * vec2(300.0, 900.0));
    fibres = mix(fibres, 0.5, smoothstep(0.0005, 0.002, footprint));
    return color * (0.9 + 0.16 * fibres + 0.1 * (Fbm(p * 8.0) - 0.5));
}

// Leather: a pebbled grain and darker creases.
vec3 Leather(vec3 color, vec2 p, float footprint)
{
    float grain = mix(Noise(p * 420.0), 0.5, smoothstep(0.0008, 0.003, footprint));
    float crease = pow(1.0 - abs(Noise(p * 60.0 + Fbm(p * 9.0) * 1.5) * 2.0 - 1.0), 14.0);
    crease *= smoothstep(0.004, 0.0015, footprint) * (0.4 + 0.6 * Noise(p * 6.0));
    return color * (0.9 + 0.18 * grain) * (1.0 - 0.07 * crease);
}

vec3 Surface(int pattern, vec3 color, vec2 p, float seed, float footprint)
{
    if (pattern == 1)
        return Felt(color, p, footprint);
    if (pattern == 2)
        return Leather(color, p, footprint);
    return Wood(p.yx * vec2(2.5, 12.0), color, seed, footprint);
}

void main()
{
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing)
        N = -N;
    vec3 V = normalize(uEye.xyz - vWorld);
    vec2 p = vWorld.xz;
    float footprint = length(fwidth(p));
    float pixel = max(footprint * 0.7, 1e-5);
    Material m = DefaultMaterial(vec3(0.5), 0.5);
    vec3 glow = vec3(0.0);
    float pulse = 0.75 + 0.25 * sin(uCameraPosition.w * 4.0);

    if (uKind == 0) {
        int pattern = int(uFieldStyle.y + 0.5);
        m.albedo = Surface(pattern, uFieldColor, p, uFieldStyle.z, footprint);
        m.roughness = uFieldStyle.x;
        m.clearcoat = uFieldStyle.w;
        m.clearcoatRoughness = 0.1;

        // The point this part of the field belongs to, and where in it.
        float across = (abs(p.x) - uLayout.w) / uLayout.x;
        float columnInHalf = clamp(floor(across), 0.0, 5.0);
        float offset = (across - columnInHalf - 0.5) * uLayout.x;
        float along = uLayout.z - abs(p.y);
        int column = p.x > 0.0 ? 5 - int(columnInHalf) : 6 + int(columnInHalf);
        int w = p.y > 0.0 ? column : 23 - column;
        // A long narrow triangle from the edge toward the middle; its distance from the slanted sides.
        float halfWidth = max(0.0, 1.0 - along / uLayout.y) * uLayout.x * 0.46;
        float slope = uLayout.x * 0.46 / uLayout.y;
        float edge = (abs(offset) - halfWidth) / sqrt(1.0 + slope * slope);
        edge = max(edge, along - uLayout.y);
        float inside = 1.0 - smoothstep(-pixel, pixel, edge);
        vec3 pointColor = uPointColor[w % 2];
        vec3 inlay = Surface(pattern, pointColor, p + vec2(0.37, 0.71) * float(w), uFieldStyle.z + 0.5, footprint);
        m.albedo = mix(m.albedo, inlay, inside);
        m.roughness = mix(m.roughness, uPointStyle.x, inside);
        // The thin line round each point.
        float line = LineCoverage(abs(edge), uPointStyle.y, pixel) * step(along, uLayout.y + uPointStyle.y);
        m.albedo = mix(m.albedo, uLineColor, line);
        m.metallic = uPointStyle.z * line;
        m.roughness = mix(m.roughness, 0.3, line);

        vec4 marks = uPoints[w];
        float near = 1.0 - smoothstep(0.0, uLayout.y * 0.3, along);
        glow += uAccent.rgb * inside * (marks.x * (0.5 * pulse + 0.2) + marks.y * 0.18 * near);
        glow += uAccent.rgb * LineCoverage(abs(edge), 0.0014, pixel) * marks.x * 1.4;
        glow += uHintColor * inside * marks.w * (0.5 * pulse + 0.2);
        m.albedo = mix(m.albedo, mix(m.albedo, vec3(0.9, 0.8, 0.55), 0.35), marks.z * inside);
    }
    else if (uKind == 1) {
        // Polished wood or lacquer, its grain along the rims.
        vec2 grain = vUv * vec2(2.5, 2.5);
        m.albedo = uWoodStyle.z > 0.5 ? uWoodColor * (0.96 + 0.06 * Noise(p * 40.0)) : Wood(grain.yx * vec2(1.0, 4.5), uWoodColor, uWoodStyle.w, footprint);
        m.roughness = uWoodStyle.x;
        m.clearcoat = uWoodStyle.y;
        m.clearcoatRoughness = 0.07;
        m.occlusion = mix(0.6, 1.0, smoothstep(-0.03, 0.01, vWorld.y));
    }
    else if (uKind == 2) {
        m.albedo = Felt(uFeltColor, p, footprint);
        m.roughness = 0.95;
        m.occlusion = 0.7;
        // The tray glows where a checker may be borne off into it.
        float mine = p.y > 0.0 ? uTrays.x : uTrays.y;
        glow += uAccent.rgb * mine * (0.3 * pulse + 0.1);
        glow += uHintColor * (p.y > 0.0 ? max(uTrays.z, 0.0) : max(-uTrays.z, 0.0)) * (0.4 * pulse + 0.15);
    }
    else {
        int pattern = int(uTableStyle.y + 0.5);
        if (pattern == 1)
            m.albedo = Felt(uTableColor, p * 0.6, footprint);
        else if (pattern == 2)
            m.albedo = Granite(p, uTableColor, uTableStyle.z, footprint);
        else
            m.albedo = Wood(p.yx * vec2(1.2, 6.0), uTableColor, uTableStyle.z, footprint);
        m.roughness = uTableStyle.x;
        // The table darkens away from the lamp over the board.
        float r = length(max(abs(p) - vec2(0.35, 0.28), 0.0));
        m.albedo *= mix(1.0, 0.18, smoothstep(0.0, 1.6, r));
        m.occlusion = mix(0.55, 1.0, smoothstep(0.0, 0.06, r));
    }

    float spread = NormalSpread(N);
    m.roughness = WidenRoughness(m.roughness, spread);
    m.clearcoatRoughness = WidenRoughness(m.clearcoatRoughness, spread);
    float shadow = uEye.w > 0.5 ? 1.0 : Shadow(vWorld, N);
    vec3 color = Shade(m, vWorld, N, V, shadow) + glow;
    fragColor = vec4(ApplyFog(color, vWorld), 1.0);
}
