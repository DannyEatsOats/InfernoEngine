#version 450
layout(location = 0) in vec3 inNearPoint;
layout(location = 1) in vec3 inFarPoint;
layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform GridConstants {
    mat4 view;
    mat4 proj;
} push;

vec4 ComputeGrid(vec3 worldPos, float scale) {
    vec2 coord = worldPos.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    return vec4(0.4, 0.4, 0.4, 1.0 - min(line, 1.0));
}

float ComputeDepth(vec3 pos) {
    vec4 clip = push.proj * push.view * vec4(pos, 1.0);
    return clip.z / clip.w;
}

float ComputeLinearDepth(float depth) {
    float near = 0.1;
    float far = 10.0;
    return (near * far) / (far - depth * (far - near));
}

void main() {
    float t = -inNearPoint.y / (inFarPoint.y - inNearPoint.y);
    if (t < 0.0) discard;

    vec3 worldPos = inNearPoint + t * (inFarPoint - inNearPoint);

    gl_FragDepth = ComputeDepth(worldPos);

    float linearDepth = ComputeLinearDepth(gl_FragDepth);
    float normalizedDepth = linearDepth / 15.0;
    float fading = max(0.0, 0.5 - normalizedDepth);

    FragColor = ComputeGrid(worldPos, 2.0) + ComputeGrid(worldPos, 0.1);
    FragColor.a *= fading;
}
