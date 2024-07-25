#version 430

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inTangentLightPosition;
layout(location = 2) in vec3 inTangentCameraPosition;
layout(location = 3) in vec3 inTangentFragmentPosition;

layout(binding = 2) uniform texture2D tex;
layout(binding = 3) uniform texture2D normalMap;
layout(binding = 4) uniform sampler texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = texture(sampler2D(normalMap, texSampler), inTexCoord).rgb;
    normal = normalize(normal * 2.0 - 1.0);   

    vec3 color = texture(sampler2D(tex, texSampler), inTexCoord).rgb;

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * color;

    vec3 lightDirection = normalize(inTangentLightPosition - inTangentFragmentPosition);  

    float diff = max(dot(lightDirection, normal), 0.0);
    vec3 diffuse = diff * color;

    float specularStrength = 0.5;
    vec3 viewDirection = normalize(inTangentCameraPosition - inTangentFragmentPosition);
    vec3 reflectDirection = reflect(-lightDirection, normal);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);
    float spec = pow(max(dot(normal, halfwayDirection), 0.0), 32.0);
    vec3 specular = specularStrength * spec * vec3(0.2);  

    outColor = vec4(ambient + diffuse + specular, 1.0);
}