#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D albedoMetallicTexture;
layout(set = 0, binding = 1) uniform sampler2D normalRoughnessTexture;
layout(set = 0, binding = 2) uniform sampler2D depthTexture;

struct PointLight {
    vec4 positionRange;
    vec4 colorIntensity;
};

struct SpotLight {
    vec4 positionRange;
    vec4 directionInnerConeCos;
    vec4 colorIntensity;
    vec4 outerConeCos;
};

layout(std430, set = 0, binding = 4) readonly buffer PointLightBuffer {
    PointLight pointLights[];
};

layout(std430, set = 0, binding = 5) readonly buffer SpotLightBuffer {
    SpotLight spotLights[];
};

layout(std430, set = 0, binding = 6) readonly buffer TilePointLightCountBuffer {
    uint tilePointLightCounts[];
};

layout(std430, set = 0, binding = 7) readonly buffer TilePointLightIndexBuffer {
    uint tilePointLightIndices[];
};

layout(std430, set = 0, binding = 8) readonly buffer TileSpotLightCountBuffer {
    uint tileSpotLightCounts[];
};

layout(std430, set = 0, binding = 9) readonly buffer TileSpotLightIndexBuffer {
    uint tileSpotLightIndices[];
};

layout(push_constant) uniform Constants {
    mat4 inverseViewProjection;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 settings;
    uvec4 lightCounts;
} push;

vec3 ReconstructWorldPosition(float depth) {
    vec2 ndc = inTexCoord * 2.0 - 1.0;
    vec4 worldPosition =
        push.inverseViewProjection * vec4(ndc, depth, 1.0);
    return worldPosition.xyz / worldPosition.w;
}

float CalculateRangeAttenuation(float distanceSquared, float range) {
    if (range <= 0.0)
        return 0.0;

    float normalizedDistanceSquared = distanceSquared / (range * range);
    float rangeFade = clamp(
        1.0 - normalizedDistanceSquared * normalizedDistanceSquared,
        0.0, 1.0);
    return (rangeFade * rangeFade) / max(distanceSquared, 0.01);
}

void main() {
    float depth = texture(depthTexture, inTexCoord).r;
    const vec3 sceneClearColor = vec3(0.14);

    if (depth >= 1.0 - 0.000001) {
        outColor = vec4(sceneClearColor, 1.0);
        return;
    }

    vec4 albedoMetallic = texture(albedoMetallicTexture, inTexCoord);
    vec4 normalRoughness = texture(normalRoughnessTexture, inTexCoord);
    vec3 worldPosition = ReconstructWorldPosition(depth);

    if (any(isnan(worldPosition)) || any(isinf(worldPosition))) {
        outColor = vec4(sceneClearColor, 1.0);
        return;
    }

    vec3 albedo = albedoMetallic.rgb;
    vec3 normal = normalize(normalRoughness.xyz);
    float ambientIntensity = push.settings.x;
    bool hasDirectionalLight = push.settings.y > 0.5;

    vec3 lighting = albedo * ambientIntensity;

    if (hasDirectionalLight) {
        vec3 surfaceToLight = normalize(-push.lightDirection.xyz);
        float diffuseFactor = max(dot(normal, surfaceToLight), 0.0);
        vec3 radiance =
            push.lightColorIntensity.rgb * push.lightColorIntensity.a;
        lighting += albedo * radiance * diffuseFactor;
    }

    const uint maxPointLightsPerTile = 128;
    uvec2 tile = min(uvec2(gl_FragCoord.xy) / 16u,
                     push.lightCounts.zw - uvec2(1u));
    uint tileIndex = tile.y * push.lightCounts.z + tile.x;
    uint tilePointLightCount = tilePointLightCounts[tileIndex];
    uint tilePointLightOffset = tileIndex * maxPointLightsPerTile;

    for (uint i = 0; i < tilePointLightCount; ++i) {
        uint lightIndex = tilePointLightIndices[tilePointLightOffset + i];
        PointLight light = pointLights[lightIndex];
        vec3 toLight = light.positionRange.xyz - worldPosition;
        float distanceSquared = dot(toLight, toLight);
        vec3 surfaceToLight =
            toLight * inversesqrt(max(distanceSquared, 0.0001));
        float diffuseFactor = max(dot(normal, surfaceToLight), 0.0);
        float attenuation = CalculateRangeAttenuation(
            distanceSquared, light.positionRange.w);
        vec3 radiance = light.colorIntensity.rgb *
                        light.colorIntensity.a * attenuation;
        lighting += albedo * radiance * diffuseFactor;
    }

    const uint maxSpotLightsPerTile = 64;
    uint tileSpotLightCount = tileSpotLightCounts[tileIndex];
    uint tileSpotLightOffset = tileIndex * maxSpotLightsPerTile;

    for (uint i = 0; i < tileSpotLightCount; ++i) {
        uint lightIndex = tileSpotLightIndices[tileSpotLightOffset + i];
        SpotLight light = spotLights[lightIndex];
        vec3 toLight = light.positionRange.xyz - worldPosition;
        float distanceSquared = dot(toLight, toLight);
        vec3 surfaceToLight =
            toLight * inversesqrt(max(distanceSquared, 0.0001));

        vec3 lightToSurface = -surfaceToLight;
        float coneCos = dot(normalize(light.directionInnerConeCos.xyz),
                            lightToSurface);
        float coneAttenuation = smoothstep(
            light.outerConeCos.x,
            light.directionInnerConeCos.w,
            coneCos);

        float diffuseFactor = max(dot(normal, surfaceToLight), 0.0);
        float rangeAttenuation = CalculateRangeAttenuation(
            distanceSquared, light.positionRange.w);
        vec3 radiance = light.colorIntensity.rgb *
                        light.colorIntensity.a *
                        rangeAttenuation * coneAttenuation;
        lighting += albedo * radiance * diffuseFactor;
    }

    outColor = vec4(lighting, 1.0);
}
