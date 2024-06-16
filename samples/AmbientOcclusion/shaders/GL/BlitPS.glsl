char const g_BlitPS[] = R"(#version 450 core
layout(row_major) uniform;

layout(binding = 0) uniform sampler2D SourceTexture;

layout(location = 0) out vec4 f_color;

void main()
{
    f_color = texelFetch(SourceTexture, ivec2(gl_FragCoord.xy), 0);
})";
