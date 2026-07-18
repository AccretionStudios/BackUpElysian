#version 450

layout(set = 0, binding = 0) uniform sampler2D albedoSampler;
layout(set = 0, binding = 1) uniform sampler2D normalSampler;
layout(set = 0, binding = 2) uniform sampler2D worldPosSampler;

struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

struct SpotLight {
    vec3 position;
    float radius;
    vec3 direction;
    float innerAngle;
    float outerAngle;
    vec3 color;
    float intensity;
};

layout(set = 1, binding = 0) uniform Light {
    // Directional light
    vec3 dirLightDirection;
    float dirLightIntensity;
    vec3 dirLightColor;
    float ambientStrength;

    // PBR: camera position (world space)
    vec3 cameraPos;

    // Point lights
    PointLight pointLights[4];
    int numPointLights;

    // Spot lights
    SpotLight spotLights[4];
    int numSpotLights;

} lightData;

layout(location = 0) out vec4 outColor;

// ----------------------------------------------------------------------------
// PBR helper functions
// ----------------------------------------------------------------------------
const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;  // direct lighting
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------------------------------------------------------
void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);

    vec3 albedo     = texelFetch(albedoSampler,   coord, 0).rgb;
    vec3 normal     = texelFetch(normalSampler,   coord, 0).xyz;
    vec3 worldPos   = texelFetch(worldPosSampler, coord, 0).xyz;

    // ---------- PBR constants (you can replace with uniforms/textures later) ----------
    float metallic  = 0.0;    // 0 = dielectric, 1 = metal
    float roughness = 0.5;    // 0 = smooth, 1 = rough
    // ---------------------------------------------------------------------------------

    vec3 V = normalize(lightData.cameraPos - worldPos);
    vec3 N = normalize(normal);

    // Fresnel reflectance at normal incidence (F0)
    vec3 F0 = mix(vec3(0.04), albedo, metallic); // 0.04 = typical dielectric

    // Ambient (simple constant, no IBL)
    vec3 ambient = albedo * lightData.ambientStrength;

    // Pre‑compute NdotV for all lights (used in specular)
    float NdotV = max(dot(N, V), 0.0001);

    vec3 totalLight = ambient;

    // ---------- Directional Light (treat as a light with no attenuation) ----------
    {
        vec3 L = normalize(lightData.dirLightDirection);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            vec3 radiance = lightData.dirLightColor * lightData.dirLightIntensity;

            // Fresnel
            vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
            // NDF & Geometry
            float D = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            // Specular BRDF
            vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
            // Diffuse (energy conserving)
            vec3 kD = (1.0 - F) * (1.0 - metallic);
            vec3 diffuse = kD * albedo / PI;

            totalLight += (diffuse + specular) * radiance * NdotL;
        }
    }

    // ---------- Point Lights ----------
    for (int i = 0; i < lightData.numPointLights; ++i) {
        PointLight light = lightData.pointLights[i];
        vec3 lightToPixel = light.position - worldPos;
        float distance = length(lightToPixel);
        vec3 L = normalize(lightToPixel);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            vec3 H = normalize(V + L);
            float attenuation = 1.0 - smoothstep(0.0, light.radius, distance);
            vec3 radiance = light.color * light.intensity * attenuation;

            vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
            float D = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
            vec3 kD = (1.0 - F) * (1.0 - metallic);
            vec3 diffuse = kD * albedo / PI;

            totalLight += (diffuse + specular) * radiance * NdotL;
        }
    }

    // ---------- Spot Lights ----------
    for (int i = 0; i < lightData.numSpotLights; ++i) {
        SpotLight light = lightData.spotLights[i];
        vec3 lightToPixel = light.position - worldPos;
        float distance = length(lightToPixel);
        vec3 L = normalize(lightToPixel);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            vec3 H = normalize(V + L);

            // Cone factor
            float cosAngle = dot(normalize(light.direction), -L);
            float cosInner = cos(light.innerAngle);
            float cosOuter = cos(light.outerAngle);
            float coneFactor = smoothstep(cosOuter, cosInner, cosAngle);

            float attenuation = 1.0 - smoothstep(0.0, light.radius, distance);
            vec3 radiance = light.color * light.intensity * attenuation * coneFactor;

            vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
            float D = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
            vec3 kD = (1.0 - F) * (1.0 - metallic);
            vec3 diffuse = kD * albedo / PI;

            totalLight += (diffuse + specular) * radiance * NdotL;
        }
    }

    outColor = vec4(totalLight, 1.0);
}