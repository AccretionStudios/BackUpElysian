#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragT;
layout(location = 4) in vec3 fragB;
layout(location = 5) in vec3 fragN;

layout(set = 0, binding = 1) uniform sampler2D albedoSampler;
layout(set = 0, binding = 2) uniform sampler2D normalSampler;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;

void main() {
    outAlbedo = vec4(fragColor, 1.0) * texture(albedoSampler, fragUV);
    outWorldPos = fragWorldPos;

    // 1. Build the TBN Matrix from the incoming vectors
    mat3 TBN = mat3(normalize(fragT), normalize(fragB), normalize(fragN));
    
    // 2. Read the flat normal from the texture (ranges from 0.0 to 1.0)
    vec3 normalMap = texture(normalSampler, fragUV).rgb;
    
    // 3. Convert the texture from (0 to 1) space into (-1 to 1) space
    normalMap = normalMap * 2.0 - 1.0;
    
    // 4. Multiply the texture normal by the TBN matrix to wrap it around the 3D curve
    outNormal = normalize(TBN * normalMap);
}