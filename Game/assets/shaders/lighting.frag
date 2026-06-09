#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFinalColor;

layout(binding = 0) uniform sampler2D gBufferPosition;
layout(binding = 1) uniform sampler2D gBufferNormal;
layout(binding = 2) uniform sampler2D gBufferAlbedo;
layout(binding = 3) uniform sampler2D gBufferDepth;

void main() {
    // 1. Sample your G-Buffer attributes
    vec3 worldPos = texture(gBufferPosition, inUV).rgb;
    vec3 normal = texture(gBufferNormal, inUV).rgb;
    vec3 albedo = texture(gBufferAlbedo, inUV).rgb;

    // 2. 👇 ADJUST YOUR LIGHT PROPERTIES HERE 👇
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5)); // Direction the light is shining *from*
    vec3 lightColor = vec3(0.5, 0.85, 0.7); // Warm sunlight color (RGB)
    float lightIntensity = 1.3; // Strength of the direct light
    float ambientIntensity = 0.15; // Low background ambient light glow

    // 3. Calculate Ambient Term
    vec3 ambient = albedo * ambientIntensity;

    // 4. Calculate Diffuse (Lambertian) Term
    vec3 N = normalize(normal);
    vec3 L = normalize(lightDir);
    float diffuseFactor = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * diffuseFactor * lightColor * lightIntensity;

    // 5. Output the combined lighting to the swapchain backbuffer
    outFinalColor = vec4(ambient + diffuse, 1.0);
}
