#version 450

// Attributes matching your C++ CreateGeometryPipeline
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor; // <-- New input
layout(location = 3) in vec2 inTexCoord; // <-- Shifted to 3

// Outputs going into gbuffer.frag
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outColor; // <-- New output
layout(location = 3) out vec2 outTexCoord; // <-- Shifted to 3

layout(push_constant) uniform Constants {
    mat4 mvp;
    mat4 model;
} push;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);

    outWorldPos = vec3(push.model * vec4(inPosition, 1.0));
    outNormal = mat3(push.model) * inNormal;
    outColor = inColor; // <-- Forward color data
    outTexCoord = inTexCoord;
}
