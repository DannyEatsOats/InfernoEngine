#version 450

// Inputs from gbuffer.vert
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor; // <-- New input
layout(location = 3) in vec2 inTexCoord; // <-- Shifted to 3

// Render targets (G-Buffer textures)
layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

void main() {
    outPosition = vec4(inWorldPos, 1.0);
    outNormal = vec4(normalize(inNormal), 1.0);

    // Write the actual mesh vertex color to the Albedo attachment!
    outAlbedo = vec4(inColor, 1.0);
}
