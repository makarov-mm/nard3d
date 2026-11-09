// The studio behind the table, out of focus.

layout(binding = 3) uniform samplerCube uPrefiltered;

in vec2 vUv;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 far = uInverseViewProjection * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 direction = normalize(far.xyz / far.w - uCameraPosition.xyz);
    vec3 color = textureLod(uPrefiltered, direction, uEnvironment.z * 0.6).rgb * uEnvironment.w;
    // Below the horizon the table fades into the fog.
    color = mix(color, uFog.rgb, smoothstep(0.02, -0.25, direction.y));
    fragColor = vec4(color, 1.0);
}
