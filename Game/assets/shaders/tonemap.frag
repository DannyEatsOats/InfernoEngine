#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrSceneColor;

layout(push_constant) uniform Constants {
    float exposure;
} push;

vec3 ACESFilm(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) /
                 (color * (c * color + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdrColor = texture(hdrSceneColor, inTexCoord).rgb;
    vec3 mappedColor = ACESFilm(hdrColor * push.exposure);
    outColor = vec4(mappedColor, 1.0);
}
