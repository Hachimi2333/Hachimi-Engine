#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

// Per-view constants, uploaded once per view instead of once per draw. The member order and
// padding must stay in sync with FrameUniforms in Renderer/FrameUniforms.h: every member is
// a mat4 or a vec4, which is what makes the std140 layout match the C++ struct.
layout(std140, binding = 0) uniform FrameBlock
{
    mat4 u_ViewProjection;
    mat4 u_DirectionalLightViewProjection;
    vec4 u_CameraPosition;            // xyz, w unused
    vec4 u_AmbientColor;              // rgb, w = intensity
    vec4 u_DirectionalLightDirection; // xyz, w = intensity
    vec4 u_DirectionalLightColor;     // rgb, w = shadow bias
    vec4 u_PointLightPosition[4];     // xyz, w = range
    vec4 u_PointLightColor[4];        // rgb, w = intensity
    vec4 u_SceneParams;               // x = point light count, y = shadows enabled,
                                      // z = environment enabled, w = environment intensity
};

// Per-draw constants.
uniform mat4 u_Model;

out vec3 v_WorldPosition;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;

void main()
{
    vec4 worldPosition = u_Model * vec4(a_Position, 1.0);
    v_WorldPosition = worldPosition.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_TexCoord = a_TexCoord;
    v_Color = a_Color;
    gl_Position = u_ViewProjection * worldPosition;
}
#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Color;

// Same block as the vertex stage; see the comment there for the packing rules.
layout(std140, binding = 0) uniform FrameBlock
{
    mat4 u_ViewProjection;
    mat4 u_DirectionalLightViewProjection;
    vec4 u_CameraPosition;
    vec4 u_AmbientColor;
    vec4 u_DirectionalLightDirection;
    vec4 u_DirectionalLightColor;
    vec4 u_PointLightPosition[4];
    vec4 u_PointLightColor[4];
    vec4 u_SceneParams;
};

uniform sampler2D u_AlbedoTexture;
uniform int u_HasAlbedoTexture;

uniform vec4 u_AlbedoColor;
uniform float u_Roughness;
uniform float u_Metallic;

uniform sampler2D u_DirectionalShadowMap;
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilteredMap;

const int MaxPointLights = 4;
const float PI = 3.14159265359;

float DistributionGGX(vec3 normal, vec3 halfDirection, float roughness)
{
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float normalDotHalf = max(dot(normal, halfDirection), 0.0);
    float denominator = normalDotHalf * normalDotHalf * (roughness4 - 1.0) + 1.0;
    return roughness4 / (PI * denominator * denominator);
}

float GeometrySchlickGGX(float normalDotDirection, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return normalDotDirection / (normalDotDirection * (1.0 - k) + k);
}

float GeometrySmith(float normalDotView, float normalDotLight, float roughness)
{
    return GeometrySchlickGGX(normalDotView, roughness) * GeometrySchlickGGX(normalDotLight, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculateLight(vec3 normal, vec3 viewDirection, vec3 lightDirection, vec3 radiance, vec3 albedo, float roughness, float metallic)
{
    float normalDotLight = max(dot(normal, lightDirection), 0.0);
    if (normalDotLight <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float normalDotView = max(dot(normal, viewDirection), 0.0);
    float normalDotHalf = max(dot(normal, halfDirection), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float distribution = DistributionGGX(normal, halfDirection, roughness);
    float geometry = GeometrySmith(normalDotView, normalDotLight, roughness);
    vec3 fresnel = FresnelSchlick(max(dot(halfDirection, viewDirection), 0.0), F0);

    vec3 specular = distribution * geometry * fresnel / max(4.0 * normalDotView * normalDotLight, 0.001);
    vec3 diffuseFactor = (vec3(1.0) - fresnel) * (1.0 - metallic);
    return (diffuseFactor * albedo / PI + specular) * radiance * normalDotLight;
}

float CalculateDirectionalShadow(vec3 worldPosition, vec3 normal, vec3 lightDirection)
{
    vec4 lightSpacePosition = u_DirectionalLightViewProjection * vec4(worldPosition, 1.0);
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;

    if (projected.x <= 0.0 || projected.x >= 1.0 || projected.y <= 0.0 || projected.y >= 1.0)
    {
        return 1.0;
    }

    float currentDepth = projected.z;
    float slopeScale = clamp(1.0 - max(dot(normal, lightDirection), 0.0), 0.0, 1.0);
    float bias = u_DirectionalLightColor.w + slopeScale * 0.0015;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_DirectionalShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 sampleCoord = projected.xy + vec2(x, y) * texelSize;
            float shadowDepth = texture(u_DirectionalShadowMap, sampleCoord).r;
            shadow += currentDepth - bias > shadowDepth ? 1.0 : 0.0;
        }
    }

    return 1.0 - shadow / 9.0;
}

void main()
{
    vec4 sampledAlbedo = u_HasAlbedoTexture == 1 ? texture(u_AlbedoTexture, v_TexCoord) : vec4(1.0);
    vec3 albedo = sampledAlbedo.rgb * u_AlbedoColor.rgb * v_Color.rgb;
    vec3 normal = normalize(v_Normal);
    vec3 viewDirection = normalize(u_CameraPosition.xyz - v_WorldPosition);

    float roughness = clamp(u_Roughness, 0.04, 1.0);
    float metallic = clamp(u_Metallic, 0.0, 1.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 lighting;
    if (u_SceneParams.z > 0.5)
    {
        vec3 irradiance = texture(u_IrradianceMap, normal).rgb;
        vec3 diffuseIBL = irradiance * albedo * (1.0 - metallic);

        vec3 reflection = reflect(-viewDirection, normal);
        vec3 prefilteredColor = textureLod(u_PrefilteredMap, reflection, roughness * 3.0).rgb;
        vec3 fresnel = FresnelSchlick(max(dot(normal, viewDirection), 0.0), F0);
        vec3 specularIBL = prefilteredColor * fresnel;

        lighting = (diffuseIBL + specularIBL) * u_SceneParams.w;
    }
    else
    {
        lighting = albedo * u_AmbientColor.rgb * u_AmbientColor.w;
    }

    vec3 directionalDirection = normalize(-u_DirectionalLightDirection.xyz);
    vec3 directionalRadiance = u_DirectionalLightColor.rgb * u_DirectionalLightDirection.w;

    float shadowFactor = 1.0;
    if (u_SceneParams.y > 0.5)
    {
        shadowFactor = CalculateDirectionalShadow(v_WorldPosition, normal, directionalDirection);
    }

    lighting += CalculateLight(normal, viewDirection, directionalDirection, directionalRadiance, albedo, roughness, metallic) * shadowFactor;

    int pointLightCount = int(u_SceneParams.x);
    for (int i = 0; i < pointLightCount && i < MaxPointLights; ++i)
    {
        vec3 offset = u_PointLightPosition[i].xyz - v_WorldPosition;
        float distance = length(offset);
        float range = max(u_PointLightPosition[i].w, 0.01);
        float distanceSquared = max(distance * distance, 0.01);

        // Smooth range window removes the hard cutoff while keeping lighting local.
        float rangeWindow = pow(clamp(1.0 - pow(distance / range, 4.0), 0.0, 1.0), 2.0);
        float attenuation = u_PointLightColor[i].w * rangeWindow / distanceSquared;

        vec3 pointRadiance = u_PointLightColor[i].rgb * attenuation;
        lighting += CalculateLight(normal, viewDirection, normalize(offset), pointRadiance, albedo, roughness, metallic);
    }

    o_Color = vec4(lighting, sampledAlbedo.a * u_AlbedoColor.a * v_Color.a);
}
