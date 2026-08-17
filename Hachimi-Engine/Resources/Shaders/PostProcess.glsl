#type vertex
#version 460 core

out vec2 v_TexCoord;

void main()
{
    const vec2 positions[3] = vec2[]
    (
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 position = positions[gl_VertexID];
    v_TexCoord = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SceneTexture;
uniform float u_Exposure;

// Narkowicz ACES approximation, keeps saturated highlights pleasant instead of clipping.
vec3 ACESToneMap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 color = texture(u_SceneTexture, v_TexCoord).rgb;
    color *= u_Exposure;
    color = ACESToneMap(color);
    color = pow(color, vec3(1.0 / 2.2));

    o_Color = vec4(color, 1.0);
}
