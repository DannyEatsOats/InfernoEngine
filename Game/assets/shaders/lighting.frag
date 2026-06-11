#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFinalColor;

layout(binding = 0) uniform sampler2D gBufferPosition;
layout(binding = 1) uniform sampler2D gBufferNormal;
layout(binding = 2) uniform sampler2D gBufferAlbedo;
layout(binding = 3) uniform sampler2D gBufferDepth;

void main() {
    vec3 worldPos = texture(gBufferPosition, inUV).rgb;
    vec3 normal = texture(gBufferNormal, inUV).rgb;
    vec3 albedo = texture(gBufferAlbedo, inUV).rgb;

    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 lightColor = vec3(0.78, 0.65, 0.5);
    float lightIntensity = 3.5;
    float ambientIntensity = 0.15;

    vec3 ambient = albedo * ambientIntensity;

    vec3 N = normalize(normal);
    vec3 L = normalize(lightDir);
    float diffuseFactor = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * diffuseFactor * lightColor * lightIntensity;

    outFinalColor = vec4(ambient + diffuse, 1.0);
}
