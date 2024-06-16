char const g_AttributesPS[] = R"(#version 450 core
layout(row_major) uniform;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_positionWS;

layout(std140, binding = 1) uniform MaterialConstants
{
    vec4 g_BaseColor;
    float g_Metallic;
    float g_Roughness;
};

layout(location = 1) out vec4 f_GBufferA;
layout(location = 0) out vec4 f_GBufferC;

void main()
{
    vec3 normal = normalize(v_normal);
    float roughness = g_Roughness;
    vec3 base_color = g_BaseColor.xyz;
    float metallic = g_Metallic;

    f_GBufferA = vec4(normal, roughness);
    f_GBufferC = vec4(base_color, metallic);
})";
