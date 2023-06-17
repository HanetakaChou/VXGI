char const g_DefaultVS[] = R"(#version 450 core
layout(row_major) uniform;

layout(std140, binding = 0) uniform GlobalConstants
{
    mat4x4 g_ViewProjMatrix;
};

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texCoord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec3 a_tangent;
layout(location = 4) in vec3 a_binormal;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_texCoord;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec3 v_tangent;
layout(location = 3) out vec3 v_binormal;
layout(location = 4) out vec3 v_positionWS;

void main()
{
    gl_Position = vec4(a_position.xyz, 1.0) * g_ViewProjMatrix;
    v_positionWS = a_position.xyz;

    v_texCoord = a_texCoord;
    v_normal = a_normal;
    v_tangent = a_tangent;
    v_binormal = a_binormal;
})";
