#version 430

layout(location = 0) in vec3 inFragPosition;
layout(location = 1) in vec2 inGlobalUV;
layout(location = 2) in vec2 inUV;

layout(binding = 3) uniform texture2D textures;
layout(binding = 4) uniform sampler texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    float noise = texture(sampler2D(textures, texSampler), inUV).r;

    vec3 colorLow = vec3(0.0, 0.2 * noise, 0.0);
    vec3 colorHigh = vec3(noise, noise, noise);

    if (inFragPosition.y < 50) {
        outColor = vec4(colorLow, 1.0);
    }
    else {
        float bias = ((inFragPosition.y - 50) / 125) * texture(sampler2D(textures, texSampler), inGlobalUV).r;
        outColor = vec4(colorLow * (1 - bias) + colorHigh * bias, 1.0);
    }
}