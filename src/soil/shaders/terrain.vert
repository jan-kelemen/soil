#version 430

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(binding = 0) uniform Transform {
    mat4 model;
    mat4 view;
    mat4 projection;
} transform;

layout(location = 0) out vec2 outTexCoord;

void main() {
    gl_Position = transform.projection * transform.view * transform.model * vec4(inPosition, 1.0);

    outTexCoord = inTexCoord;
}

