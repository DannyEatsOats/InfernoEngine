#version 450

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outAlbedoMetallic;
layout(location = 1) out vec4 outNormalRoughness;
layout(location = 2) out uint outEntityID;

layout(set = 0, binding = 0) uniform sampler2D albedoTexture;

layout(push_constant) uniform Constants {
    mat4 mvp;
    mat4 model;
    uint id;
} push;

void main() {
    const float metallic = 0.0;
    const float roughness = 0.7;

    vec3 albedo = texture(albedoTexture, inTexCoord).rgb;

    outAlbedoMetallic = vec4(albedo, metallic);
    outNormalRoughness = vec4(normalize(inWorldNormal), roughness);
    outEntityID = push.id;
}
