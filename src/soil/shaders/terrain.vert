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

layout(std430, binding = 3) readonly buffer NormalBuffer {
    vec4 normals[];
} normal;

layout(location = 0) out vec3 outFragPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outGlobalUV;
layout(location = 3) out vec2 outUV;

uvec2 globalPosition() {
    uint chunkY = pushConsts.chunk / pushConsts.chunksPerDimension;
    uint chunkX = pushConsts.chunk % pushConsts.chunksPerDimension;

    return uvec2(chunkX * (pushConsts.chunkDimension - 1) + inChunkPosition.x, chunkY * (pushConsts.chunkDimension - 1) + inChunkPosition.y);
}

void main() {
    uvec2 globalPos = globalPosition();

    uint vertexIndex = globalPos.y * pushConsts.terrainDimension + globalPos.x;
    vec4 vertex = vec4(inChunkPosition.x, heightmap.heights[vertexIndex], inChunkPosition.y, 1.0);

    mat4 model = chunks.chunks[pushConsts.chunk].model;
    vec4 worldPosition = model * vertex;

    gl_Position = camera.projection * camera.view * worldPosition;

    outFragPosition = vec3(globalPos.x, vertex.y, globalPos.y);
    outNormal = (model * normal.normals[vertexIndex]).xyz;
    outGlobalUV = vec2(float(globalPos.x) / pushConsts.terrainDimension, float(globalPos.y) / pushConsts.terrainDimension);
    outUV = vec2(float(inChunkPosition.x) / (pushConsts.chunkDimension - 1), float(inChunkPosition.y) / (pushConsts.chunkDimension - 1));
}

