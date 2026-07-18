#version 450

layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;
layout(location = 2) out vec3 viewNear;
layout(location = 3) out vec3 viewFar;
layout(location = 4) out vec4 clipNear;
layout(location = 5) out vec4 clipFar;

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
} pcs;

const vec2 gridPlane[6] = vec2[](
    vec2( 1.0,  1.0), vec2(-1.0, -1.0), vec2(-1.0,  1.0),
    vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2( 1.0, -1.0)
);

void main() {
    mat4 MVP = pcs.proj * pcs.view;
    mat4 invMVP = inverse(MVP);

    vec2 p = gridPlane[gl_VertexIndex];

    // Unproject to world space
    vec4 nearH = invMVP * vec4(p.x, p.y, 0.0, 1.0);
    vec4 farH  = invMVP * vec4(p.x, p.y, 1.0, 1.0);
    nearPoint = nearH.xyz / nearH.w;
    farPoint  = farH.xyz / farH.w;

    // Pre‑compute clip‑space and view‑space positions for interpolation
    clipNear = MVP * vec4(nearPoint, 1.0);
    clipFar  = MVP * vec4(farPoint,  1.0);
    viewNear = (pcs.view * vec4(nearPoint, 1.0)).xyz;
    viewFar  = (pcs.view * vec4(farPoint,  1.0)).xyz;

    gl_Position = vec4(p, 0.0, 1.0);
}