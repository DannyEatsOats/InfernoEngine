#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldNormal;
layout(location = 1) out vec2 outTexCoord;

layout(push_constant) uniform Constants {
    mat4 mvp;
    mat4 model;
    uint id;
} push;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    outWorldNormal = normalize(normalMatrix * inNormal);
    outTexCoord = inTexCoord;
}
