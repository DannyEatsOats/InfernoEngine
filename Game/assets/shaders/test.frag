#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec4 outFinalColor;
layout(location = 1) flat out uint outEntityID;

layout(set = 0, binding = 0) uniform sampler2D albedoTexture;

layout(push_constant) uniform Constants {
    mat4 mvp;
    mat4 model;
    uint id;
} push;

void main() {
    outFinalColor = texture(albedoTexture, inTexCoord);
    outEntityID = push.id;
}
