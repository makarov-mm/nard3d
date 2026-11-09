// Directions of cube map texels, and sample patterns, for the light probes.

const float PI = 3.14159265359;

vec3 CubeDirection(ivec3 texel, float size)
{
    vec2 st = (vec2(texel.xy) + 0.5) / size * 2.0 - 1.0;
    switch (texel.z) {
    case 0: return normalize(vec3(1.0, -st.y, -st.x));
    case 1: return normalize(vec3(-1.0, -st.y, st.x));
    case 2: return normalize(vec3(st.x, 1.0, st.y));
    case 3: return normalize(vec3(st.x, -1.0, -st.y));
    case 4: return normalize(vec3(st.x, -st.y, 1.0));
    default: return normalize(vec3(-st.x, -st.y, -1.0));
    }
}

vec2 Hammersley(uint i, uint count)
{
    return vec2(float(i) / float(count), float(bitfieldReverse(i)) * 2.3283064365386963e-10);
}

mat3 TangentFrame(vec3 n)
{
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    return mat3(tangent, cross(n, tangent), n);
}

vec3 ImportanceSampleGgx(vec2 xi, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float DistributionGgx(float NoH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
