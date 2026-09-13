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

uniform sampler2D u_SourceTexture;

// Straight copy. The source is the display image PostProcess.glsl already tone-mapped and
// gamma-encoded, so presenting it must not touch a single value.
void main()
{
    o_Color = vec4(texture(u_SourceTexture, v_TexCoord).rgb, 1.0);
}
