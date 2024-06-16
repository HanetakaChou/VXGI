char const g_DefaultVS[] = R"(#version 450 core
layout(row_major) uniform;

layout(std140, binding = 0) uniform GlobalConstants
{
    mat4x4 g_ViewProjMatrix;
    mat4x4 g_ViewProjMatrixInv;
    mat4x4 g_LightViewProjMatrix;
    vec4 g_LightPos;
    vec4 g_LightColor;
    vec4 g_AmbientColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
};

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_positionWS;

void main()
{
    gl_Position = vec4(a_position.xyz, 1.0) * g_ViewProjMatrix;
    v_positionWS = a_position.xyz;

    v_normal = a_normal;
})";
