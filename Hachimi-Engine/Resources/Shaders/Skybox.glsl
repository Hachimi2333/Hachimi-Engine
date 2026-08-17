#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;

out vec3 v_Direction;

void main()
{
    v_Direction = a_Position;
    vec4 clipPosition = u_ViewProjection * vec4(a_Position, 1.0);
    gl_Position = clipPosition.xyww;
}
#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;

in vec3 v_Direction;

uniform samplerCube u_SkyboxTexture;
uniform float u_SkyboxIntensity;

void main()
{
    vec3 color = texture(u_SkyboxTexture, normalize(v_Direction)).rgb * u_SkyboxIntensity;
    o_Color = vec4(color, 1.0);
}
