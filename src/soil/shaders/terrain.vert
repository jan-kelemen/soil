#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inTangent;

layout(binding = 0) uniform Transform {
    mat4 model;
    mat4 view;
    mat4 projection;
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
    vec3 normal = vec3(0.0, 0.0, 1.0);

    vec3 fragmentPosition = (transform.model * vec4(inPosition, 1.0)).xyz;

    outTexCoord = inTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(transform.model)));
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 N = normalize(normalMatrix * normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    
    mat3 TBN = transpose(mat3(T, B, N));    
    outTangentLightPosition = TBN * view.lightPosition;
    outTangentCameraPosition  = TBN * view.cameraPosition;
    outTangentFragmentPosition  = TBN * fragmentPosition;

    gl_Position = transform.projection * transform.view * transform.model * vec4(inPosition, 1.0);
}

