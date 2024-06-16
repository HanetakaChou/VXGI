char const g_CompositingPS[] = R"(#version 450 core
layout(row_major) uniform;

layout(binding = 0) uniform sampler2D t_VXAO;
layout(binding = 1) uniform sampler2D t_GBufferGBufferC;

layout(location = 0) out vec4 f_color;

void main()
{
    ivec2 pixelPos = ivec2(gl_FragCoord.xy);

    float vxao = texelFetch(t_VXAO, pixelPos, 0).x;
    float ao = vxao;

    vec4 GBufferC = texelFetch(t_GBufferGBufferC, pixelPos, 0);
    vec3 base_color = GBufferC.xyz;

    f_color = vec4(base_color * ao, 1.0);
})";
