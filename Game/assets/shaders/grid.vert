#version 450

layout(location = 0) out vec3 outNearPoint;
layout(location = 1) out vec3 outFarPoint;

layout(push_constant) uniform GridConstants {
    mat4 viewProjection;
    mat4 inverseViewProjection;
} push;

vec3 positions[6] = vec3[](
        vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
        vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
    );

vec3 Unproject(float x, float y, float z) {
    vec4 p = push.inverseViewProjection * vec4(x, y, z, 1.0);
    return p.xyz / p.w;
}

void main() {
    vec3 point = positions[gl_VertexIndex];
    outNearPoint = Unproject(point.x, point.y, 0.0);
    outFarPoint = Unproject(point.x, point.y, 1.0);
    gl_Position = vec4(point, 1.0);
}
