#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outColor;
layout(location = 3) out vec2 outTexCoord;

layout(push_constant) uniform Constants {
    mat4 mvp;
    mat4 model;
} push;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);

    outWorldPos = vec3(push.model * vec4(inPosition, 1.0));
    outNormal = mat3(push.model) * inNormal;
    outColor = inColor;
    outTexCoord = inTexCoord;
}
