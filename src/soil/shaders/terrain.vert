#version 460

layout(location = 0) in vec2 inPosition;

layout(push_constant) uniform PushConsts {
    uint lod;
} pushConsts;

layout(binding = 0) uniform Transform {
    mat4 model;
    mat4 view;
    mat4 projection;
} transform;

layout(std430, binding = 1) readonly buffer Heightmap {
    float heights[];
} heightmap;

layout(location = 0) out vec3 outColor;

void main() {
    vec4 vertex = vec4(inPosition.x, heightmap.heights[gl_VertexIndex] - pushConsts.lod * 10, inPosition.y, 1.0);

    vec4 worldPosition = transform.model * vertex;

    gl_Position = transform.projection * transform.view * worldPosition;
    outColor = vec3(0.5, heightmap.heights[gl_VertexIndex], 0.5);
}

