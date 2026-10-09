#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D albedoMetallicTexture;
layout(set = 0, binding = 1) uniform sampler2D normalRoughnessTexture;
layout(set = 0, binding = 2) uniform sampler2D depthTexture;
layout(set = 0, binding = 3) uniform usampler2D entityIDTexture;

layout(push_constant) uniform Constants {
    mat4 inverseProjection;
    float nearPlane;
    float farPlane;
    uint viewMode;
} push;

void main() {
    vec4 albedoMetallic = texture(albedoMetallicTexture, inTexCoord);
    vec4 normalRoughness = texture(normalRoughnessTexture, inTexCoord);
    float depth = texture(depthTexture, inTexCoord).r;
    uint entityID = texture(entityIDTexture, inTexCoord).r;

    const vec3 sceneClearColor = vec3(0.14);
    if (depth >= 1.0 - 0.000001) {
        outColor = vec4(sceneClearColor, 1.0);
        return;
    }

    switch (push.viewMode) {
        case 1:
            outColor = vec4(albedoMetallic.rgb, 1.0);
            break;
        case 2:
            outColor = vec4(normalize(normalRoughness.xyz) * 0.5 + 0.5, 1.0);
            break;
        case 3:
            outColor = vec4(vec3(albedoMetallic.a), 1.0);
            break;
        case 4:
            outColor = vec4(vec3(normalRoughness.a), 1.0);
            break;
        case 5:
            vec2 ndc = inTexCoord * 2.0 - 1.0;
            vec4 viewPosition =
                push.inverseProjection * vec4(ndc, depth, 1.0);
            float viewDepth = abs(viewPosition.z / viewPosition.w);
            float visualizationDistance = min(push.farPlane, 50.0);
            float linearDepth = clamp(
                (viewDepth - push.nearPlane) /
                    max(visualizationDistance - push.nearPlane, 0.0001),
                0.0,
                1.0);
            outColor = vec4(vec3(linearDepth), 1.0);
            break;
        case 6:
            if (entityID == 0) {
                outColor = vec4(0.0, 0.0, 0.0, 1.0);
                break;
            }

            uint hash = entityID * 747796405u + 2891336453u;
            hash = ((hash >> ((hash >> 28u) + 4u)) ^ hash) * 277803737u;
            hash = (hash >> 22u) ^ hash;

            vec3 entityColor = vec3(
                hash & 255u,
                (hash >> 8u) & 255u,
                (hash >> 16u) & 255u) / 255.0;
            outColor = vec4(entityColor, 1.0);
            break;
        default:
            outColor = vec4(1.0, 0.0, 1.0, 1.0);
            break;
    }
}
