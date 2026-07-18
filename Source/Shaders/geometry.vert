#version 450

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(set = 0, binding = 0) uniform ViewProj {
    mat4 view;
    mat4 proj;
} vp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec3 inTangent; // --- RECEIVED FROM C++ ---

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec2 fragUV;

// Passing T, B, and N directly to the fragment shader
layout(location = 3) out vec3 fragT;
layout(location = 4) out vec3 fragB;
layout(location = 5) out vec3 fragN;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = vp.proj * vp.view * worldPos;
    
    fragColor = inColor;
    fragWorldPos = worldPos.xyz;
    fragUV = inUV;

    // --- CONSTRUCT THE TBN VECTORS ---
    vec3 T = normalize(mat3(pc.model) * inTangent);
    vec3 N = normalize(mat3(pc.model) * inNormal);
    
    // Re-orthogonalize T with respect to N (Gram-Schmidt) just to be perfectly safe
    T = normalize(T - dot(T, N) * N);
    
    // The bitangent is simply the cross product of the normal and tangent
    vec3 B = cross(N, T);
    
    fragT = T;
    fragB = B;
    fragN = N;
}