#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(binding = 0) uniform Transform {
    mat4 model;
    mat4 view;
    mat4 projection;

    mat4 normal;
} transform;

layout(binding = 1) uniform View {
    vec3 cameraPosition;
    vec3 lightPosition;
} view;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outTangentLightPosition;
layout(location = 2) out vec3 outTangentCameraPosition;
layout(location = 3) out vec3 outTangentFragmentPosition;

void main() {
    vec4 worldPosition = transform.model * vec4(inPosition, 1.0);

    outTexCoord = inTexCoord;

    mat3 normalMatrix = mat3(transform.normal);
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 N = normalize(normalMatrix * inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    
    mat3 TBN = transpose(mat3(T, B, N));    
    outTangentLightPosition = TBN * view.lightPosition;
    outTangentCameraPosition  = TBN * view.cameraPosition;
    outTangentFragmentPosition  = TBN * worldPosition.xyz;

    gl_Position = transform.projection * transform.view * worldPosition;
}

