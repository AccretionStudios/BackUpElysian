#version 450

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 2) in vec3 viewNear;
layout(location = 3) in vec3 viewFar;
layout(location = 4) in vec4 clipNear;
layout(location = 5) in vec4 clipFar;

layout(location = 0) out vec4 outColor;

const float MAX_DIST = 200.0;
const float EPS      = 1e-6;

const vec3 MINOR_COLOR = vec3(0.12);
const vec3 MAJOR_COLOR = vec3(0.55);
const vec3 AXIS_RED    = vec3(1.0, 0.1, 0.1);
const vec3 AXIS_BLUE   = vec3(0.1, 0.2, 1.0);

void main() {
    // ---- Ray–plane intersection (Y = 0) ----
    float t = -nearPoint.y / (farPoint.y - nearPoint.y + EPS);
    if (t < 0.0) discard;

    // ---- Interpolate all attributes at the hit point ----
    vec3 worldPos = mix(nearPoint, farPoint, t);
    vec3 viewPos  = mix(viewNear, viewFar, t);
    vec4 clipPos  = mix(clipNear, clipFar, t);

    // ---- Write depth ----
    gl_FragDepth = clipPos.z / clipPos.w;

    // ---- Distance & horizon fade ----
    float depth = -viewPos.z;
    float distFade   = 1.0 - smoothstep(0.0, MAX_DIST, depth);
    float horizonFade = clamp((5.0 - fwidth(viewPos.z)) / 4.5, 0.0, 1.0);
    float finalFade = distFade * horizonFade;
    if (finalFade < 0.001) discard;

    // ---- Grid line anti‑aliasing (1 m and 10 m) ----
    vec2 worldXZ = worldPos.xz;
    vec2 deriv = fwidth(worldXZ);

    // Inverse derivatives (for computing pixel‑space line widths)
    vec2 invDeriv   = 1.0 / max(deriv, EPS);
    vec2 invDeriv10 = 1.0 / max(deriv * 0.1, EPS); // exact, keeps original behaviour

    // 1 m lines
    vec2 dist1 = abs(fract(worldXZ - 0.5) - 0.5);
    float alpha1m = max(0.0, 1.0 - min(dist1.x * invDeriv.x, dist1.y * invDeriv.y));

    // 10 m lines
    vec2 p10 = worldXZ * 0.1;
    vec2 dist10 = abs(fract(p10 - 0.5) - 0.5);
    float alpha10m = max(0.0, 1.0 - min(dist10.x * invDeriv10.x, dist10.y * invDeriv10.y));

    float gridAlphaCombined = min(alpha1m + alpha10m * 2.0, 1.0);

    // ---- Axes (X = 0 and Z = 0) ----
    float onZ = max(0.0, 1.0 - abs(worldPos.z) * invDeriv.y);
    float onX = max(0.0, 1.0 - abs(worldPos.x) * invDeriv.x);
    float axisActive = max(onZ, onX * (1.0 - onZ));

    // Early discard if final alpha is too low (avoids colour calculations)
    float finalAlpha = max(gridAlphaCombined, axisActive) * finalFade;
    if (finalAlpha < 0.001) discard;

    // ---- Final colour blend ----
    vec3 gridColor = MINOR_COLOR * alpha1m + MAJOR_COLOR * alpha10m * 2.0;
    vec3 axisColor = AXIS_RED * onZ + AXIS_BLUE * onX * (1.0 - onZ);
    vec3 finalColor = mix(gridColor, axisColor, axisActive);

    outColor = vec4(finalColor, finalAlpha);
}