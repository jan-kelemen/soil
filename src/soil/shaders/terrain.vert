#version 460

layout(location = 0) in uvec2 inChunkPosition;

layout(push_constant) uniform PushConsts {
    uint lod;
    uint chunk;
    uint chunkDimension;
    uint terrainDimension;
    uint chunksPerDimension;
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

uvec2 globalPosition() {
    uint chunkY = pushConsts.chunk / pushConsts.chunksPerDimension;
    uint chunkX = pushConsts.chunk % pushConsts.chunksPerDimension;

    return uvec2(chunkX * (pushConsts.chunkDimension - 1) + inChunkPosition.x, chunkY * (pushConsts.chunkDimension - 1) + inChunkPosition.y);
}

void main() {
    uvec2 globalPos = globalPosition();

    vec4 vertex = vec4(inChunkPosition.x, heightmap.heights[globalPos.y * pushConsts.terrainDimension + globalPos.x], inChunkPosition.y, 1.0);
    mat4 model = chunks.chunks[pushConsts.chunk].model;

    vec4 worldPosition = model * vertex;

    gl_Position = camera.projection * camera.view * worldPosition;
    outColor = vec3(0.5, vertex.y / 255, 0.5);
}

