#version 450

layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform usampler2D entityBuffer;

layout(push_constant) uniform PushConstants{
    vec3 color;
    uint entityID;
    int thicknessPx;
}push;

bool isOffscreen(ivec2 samplePos, ivec2 texelSize){
    return any(lessThan(samplePos, ivec2(0))) || any(greaterThanEqual(samplePos, texelSize));
}

bool isBackground(ivec2 samplePos, ivec2 texelSize){
    if(isOffscreen(samplePos, texelSize)){
        return true;
    }
    uint neighbourID = texelFetch(entityBuffer, samplePos, 0).r;
    return neighbourID != push.entityID;
}

int distanceToBorder(ivec2 pixelCoord, ivec2 dir, ivec2 texelSize){
    if(dir.x > 0) return texelSize.x - 1 - pixelCoord.x;
    if(dir.x < 0) return pixelCoord.x;
    if(dir.y > 0) return texelSize.y - 1 - pixelCoord.y;
    return pixelCoord.y;
}

bool entityTouchesBorder(ivec2 pixelCoord, ivec2 dir, ivec2 texelSize){
    ivec2 borderPos = pixelCoord;
    if(dir.x != 0) borderPos.x = dir.x > 0 ? texelSize.x - 1 : 0;
    if(dir.y != 0) borderPos.y = dir.y > 0 ? texelSize.y - 1 : 0;
    uint borderEntity = texelFetch(entityBuffer, borderPos, 0).r;
    return borderEntity == push.entityID;
}

void main(){
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    ivec2 texelSize = textureSize(entityBuffer, 0);

    uint centerEntity = texelFetch(entityBuffer, pixelCoord, 0).r;
    bool centerSelected = (centerEntity == uint(push.entityID));

    bool edge = false;
    ivec2 dirs[4] = ivec2[](ivec2(1,0), ivec2(-1,0), ivec2(0,1), ivec2(0,-1));

    for(int i = 0; i < 4 && !edge; ++i){
        ivec2 dir = dirs[i];
        ivec2 samplePos = pixelCoord + dir * push.thicknessPx;

        if(isBackground(samplePos, texelSize) == centerSelected){
            edge = true;
            continue;
        }

        if(centerSelected
           && distanceToBorder(pixelCoord, dir, texelSize) < push.thicknessPx * 2
           && entityTouchesBorder(pixelCoord, dir, texelSize)){
            edge = true;
        }
    }

    if(edge) {
        outColor = vec4(push.color, 1.0);
    } else {
        discard;
    }
}
