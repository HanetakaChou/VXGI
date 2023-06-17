char const g_AttributesPS[] = R"(#version 450 core
layout(row_major) uniform;

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec3 v_tangent;
layout(location = 3) in vec3 v_binormal;
layout(location = 4) in vec3 v_positionWS;

layout(binding = 0) uniform sampler2D DiffuseTexture;
layout(binding = 1) uniform sampler2D SpecularTexture;
layout(binding = 2) uniform sampler2D NormalTexture;

vec3 GetNormal()
{
    vec3 pixelNormal = texture(NormalTexture, v_texCoord).xyz;
    vec3 normal = normalize(v_normal);

    if(pixelNormal.z != 0)
    {
        vec3 tangent = normalize(v_tangent);
        vec3 binormal = normalize(v_binormal);
        mat3x3 TangentMatrix = mat3x3(tangent, binormal, normal);
        normal = normalize(TangentMatrix * (pixelNormal * 2 - 1));
    }

    return normal;
}

out vec4 f_albedo;
out vec4 f_normal;

void main()
{
    vec4 diffuseColor;
    diffuseColor.rgb = texture(DiffuseTexture, v_texCoord).rgb;
    diffuseColor.a = texture(SpecularTexture, v_texCoord).r;
    vec3 normal = GetNormal();
    float roughness = (diffuseColor.a > 0) ? 0.5 : 0;

    f_albedo = diffuseColor;
    f_normal = vec4(normal.xyz, roughness);
})";
