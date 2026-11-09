// Physically based shading: a key light with soft shadows, and image-based light from a studio.

layout(binding = 0) uniform sampler2DShadow uShadowMap;
layout(binding = 1) uniform sampler2D uShadowDepth;
layout(binding = 2) uniform samplerCube uIrradiance;
layout(binding = 3) uniform samplerCube uPrefiltered;
layout(binding = 4) uniform sampler2D uBrdf;

struct Material {
    vec3 albedo;
    float roughness;
    float metallic;
    float clearcoat;
    float clearcoatRoughness;
    float subsurface;     // light wrapping round, for ivory and porcelain
    float occlusion;
    vec3 emissive;
};

Material DefaultMaterial(vec3 albedo, float roughness)
{
    Material m;
    m.albedo = albedo;
    m.roughness = roughness;
    m.metallic = 0.0;
    m.clearcoat = 0.0;
    m.clearcoatRoughness = 0.05;
    m.subsurface = 0.0;
    m.occlusion = 1.0;
    m.emissive = vec3(0.0);
    return m;
}

// How much the normal turns within the pixel. Highlights are widened by it, so that they do not sparkle
// on curved and bevelled edges (specular anti-aliasing, after Tokuyoshi and Kaplanyan). To be called where
// all the pixels of a quad take the same path.
float NormalSpread(vec3 N)
{
    vec3 dx = dFdx(N), dy = dFdy(N);
    return min(0.5 * (dot(dx, dx) + dot(dy, dy)), 0.18);
}

float WidenRoughness(float roughness, float spread)
{
    float a = roughness * roughness;
    return sqrt(sqrt(Saturate(a * a + spread)));
}

float DistributionGgx(float NoH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float VisibilitySmithGgx(float NoV, float NoL, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float lambdaV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float lambdaL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(lambdaV + lambdaL, 1e-5);
}

vec3 FresnelSchlick(vec3 f0, float cosine)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
}

float FresnelSchlick(float f0, float cosine)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
}

vec3 FresnelRoughness(vec3 f0, float NoV, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - NoV, 5.0);
}

float SpecularOcclusion(float NoV, float occlusion, float roughness)
{
    return Saturate(pow(NoV + occlusion, exp2(-16.0 * roughness - 1.0)) - 1.0 + occlusion);
}

const vec2 Poisson[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725), vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464), vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420), vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590), vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

// Percentage-closer soft shadows: the penumbra widens with the distance from the shadow's caster.
float Shadow(vec3 position, vec3 normal)
{
    float texel = uLightDirection.w;
    vec3 offsetPosition = position + normal * texel * 1.5;
    vec4 light = uLightViewProjection * vec4(offsetPosition, 1.0);
    vec3 p = light.xyz / light.w * 0.5 + 0.5;
    if (p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0 || p.z >= 1.0)
        return 1.0;

    float angle = 6.2831853 * InterleavedGradientNoise(gl_FragCoord.xy);
    mat2 rotation = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    vec2 texelUv = 1.0 / vec2(textureSize(uShadowDepth, 0));
    float bias = uShadowParams.y;

    float blockerSum = 0.0;
    float blockers = 0.0;
    for (int i = 0; i < 12; ++i) {
        float depth = textureLod(uShadowDepth, p.xy + rotation * Poisson[i] * 14.0 * texelUv, 0.0).r;
        if (depth < p.z - bias) {
            blockerSum += depth;
            blockers += 1.0;
        }
    }
    if (blockers == 0.0)
        return 1.0;

    float distance = (p.z - blockerSum / blockers) * uShadowParams.x;
    float penumbra = clamp(distance * uLightColor.w / texel, 1.2, 16.0);
    float lit = 0.0;
    for (int i = 0; i < 16; ++i)
        lit += textureLod(uShadowMap, vec3(p.xy + rotation * Poisson[i] * penumbra * texelUv, p.z - bias), 0.0);
    return lit / 16.0;
}

vec3 Shade(Material m, vec3 position, vec3 N, vec3 V, float shadow)
{
    float NoV = max(dot(N, V), 1e-4);
    vec3 f0 = mix(vec3(0.04), m.albedo, m.metallic);
    vec3 diffuseColor = m.albedo * (1.0 - m.metallic);

    // The key light.
    vec3 L = uLightDirection.xyz;
    vec3 H = normalize(V + L);
    float NoL = dot(N, L);
    float NoLc = Saturate(NoL);
    float NoH = Saturate(dot(N, H));
    float VoH = Saturate(dot(V, H));
    float wrap = m.subsurface * 0.5;
    float diffuseLight = Saturate((NoL + wrap) / ((1.0 + wrap) * (1.0 + wrap)));
    vec3 F = FresnelSchlick(f0, VoH);
    vec3 specular = DistributionGgx(NoH, m.roughness) * VisibilitySmithGgx(NoV, NoLc, m.roughness) * F * NoLc;
    vec3 diffuse = diffuseColor / PI * diffuseLight * (1.0 - F);
    // Light passing through thin parts, warmed by the material.
    diffuse += diffuseColor * m.subsurface * 0.12 * pow(Saturate(dot(V, -L)), 3.0);
    vec3 direct = (diffuse + specular) * uLightColor.rgb * shadow;

    if (m.clearcoat > 0.0) {
        float coat = FresnelSchlick(0.04, VoH) * m.clearcoat;
        float coatSpecular = DistributionGgx(NoH, m.clearcoatRoughness) * 0.25 / max(VoH * VoH, 1e-4);
        direct = direct * (1.0 - coat) + coat * coatSpecular * NoLc * uLightColor.rgb * shadow;
    }

    // The studio around.
    vec3 R = reflect(-V, N);
    vec2 brdf = texture(uBrdf, vec2(NoV, m.roughness)).rg;
    vec3 Fr = FresnelRoughness(f0, NoV, m.roughness);
    vec3 environment = textureLod(uPrefiltered, R, m.roughness * uEnvironment.z).rgb;
    float specularOcclusion = SpecularOcclusion(NoV, m.occlusion, m.roughness);
    vec3 irradiance = texture(uIrradiance, N).rgb * mix(1.0, shadow, 0.35);
    vec3 ambient = irradiance * diffuseColor * (1.0 - Fr) * m.occlusion * uEnvironment.x;
    ambient += environment * (f0 * brdf.x + brdf.y) * specularOcclusion * uEnvironment.y;

    if (m.clearcoat > 0.0) {
        float coat = FresnelSchlick(0.04, NoV) * m.clearcoat;
        vec3 coatEnvironment = textureLod(uPrefiltered, R, m.clearcoatRoughness * uEnvironment.z).rgb;
        ambient = ambient * (1.0 - coat) + coatEnvironment * coat * specularOcclusion * uEnvironment.y;
    }

    return direct + ambient + m.emissive;
}

vec3 ApplyFog(vec3 color, vec3 position)
{
    float distance = length(position - uCameraPosition.xyz);
    float amount = 1.0 - exp(-max(distance - uShadowParams.z, 0.0) * uFog.w);
    return mix(color, uFog.rgb, Saturate(amount));
}
