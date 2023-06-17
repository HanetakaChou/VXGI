char const g_CompositingPS[] = R"(#version 450 core
layout(row_major) uniform;

layout(binding = 0) uniform sampler2D AmbientOcclusion;
layout(binding = 1) uniform sampler2D GBufferAlbedo;

in vec4 gl_FragCoord;
out vec4 f_color;

void main()
{
    vec4 ao = texelFetch(AmbientOcclusion, ivec2(gl_FragCoord.xy), 0);
    vec4 albedo = texelFetch(GBufferAlbedo, ivec2(gl_FragCoord.xy), 0);
    f_color = ao * albedo;
})";
