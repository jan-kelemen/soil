#version 460

layout(location = 0) in uvec2 inPosition;

layout(push_constant) uniform PushConsts {
    uint lod;
    uint chunk;
    uint chunkDimension;
    uint terrainDimension;
} pushConsts;

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
} camera;

struct Chunk {
    mat4 model;
};

layout(std140, binding = 1) readonly buffer ChunkBuffer {
    Chunk chunks[];
} chunks;

layout(std430, binding = 2) readonly buffer Heightmap {
    float heights[];
} heightmap;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec4 vtxChk;

uint vertToHeightmap(uint vertex_index) {
    uint chunkY = pushConsts.chunk / 33;
    uint chunkX = pushConsts.chunk % 33;

    return (chunkY * pushConsts.chunkDimension + inPosition.y) * pushConsts.terrainDimension + (chunkX * pushConsts.chunkDimension + inPosition.x);
}

void main() {
    vec4 vertex = vec4(inPosition.x, heightmap.heights[vertToHeightmap(gl_VertexIndex)], inPosition.y, 1.0);
    mat4 model = chunks.chunks[pushConsts.chunk].model;

    vec4 worldPosition = model * vertex;

    gl_Position = camera.projection * camera.view * worldPosition;
    outColor = vec3(0.5, vertex.y / 255, 0.5);
    vtxChk = vec4(inPosition.x, inPosition.y, pushConsts.chunk, vertToHeightmap(gl_VertexIndex));
}

