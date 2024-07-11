#version 430

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform Transform {
    mat4 view;
    mat4 projection;
} transform;

layout(location = 0) out vec3 outColor;

void main() {
    gl_Position = transform.projection * transform.view * vec4(inPosition, 1.0);

    outColor = inColor;
}

