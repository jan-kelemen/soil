#version 430

layout(location = 0) in vec3 inFragPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inGlobalUV;
layout(location = 3) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint lod;
    uint chunk;
    uint chunkDimension;
    uint terrainDimension;
    uint chunksPerDimension;
} pushConsts;

layout(binding = 4) uniform texture2D textures;
layout(binding = 5) uniform sampler texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    float noise = texture(sampler2D(textures, texSampler), inUV).r;

    vec3 colorLow = vec3(0.0, 0.2 * noise, 0.0);
    vec3 colorHigh = vec3(noise, noise, noise);
    vec3 color;
    if (inFragPosition.y < 50) {
        color = colorLow, 1.0;
    }
    else {
        float bias = ((inFragPosition.y - 50) / 125) * texture(sampler2D(textures, texSampler), inGlobalUV).r;
        color = colorLow * (1 - bias) + colorHigh * bias;
    }

    float center = float(pushConsts.terrainDimension) / 2.0;
    vec3 lightPosition = vec3(center, 300, center);

    vec3 norm = normalize(inNormal);
    vec3 lightDirection = normalize(lightPosition - inFragPosition);

    float diff = max(dot(norm, lightDirection), 0.1);
    vec3 result = diff * color;

    outColor = vec4(result, 1.);
}