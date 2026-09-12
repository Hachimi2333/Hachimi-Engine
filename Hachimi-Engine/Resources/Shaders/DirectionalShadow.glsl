#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;

// Named to keep it distinct from the frame block's u_ViewProjection in the scene shader:
// this one holds the light's view-projection for the shadow pass.
uniform mat4 u_LightViewProjection;
uniform mat4 u_Model;

void main()
{
    gl_Position = u_LightViewProjection * u_Model * vec4(a_Position, 1.0);
}
#type fragment
#version 460 core

void main()
{
}
