char const g_VoxelizationPS[] = R"(layout(row_major) uniform;

layout(std140, binding = 0) uniform GlobalConstants
{
    mat4x4 g_ViewProjMatrix;
    mat4x4 g_ViewProjMatrixInv;
    mat4x4 g_LightViewProjMatrix;
    vec4 g_LightDirection;
    vec4 g_DiffuseColor;
    vec4 g_LightColor;
    vec4 g_AmbientColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
};

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec3 v_tangent;
layout(location = 3) in vec3 v_binormal;
layout(location = 4) in vec3 v_positionWS;

flat in VxgiVoxelizationPSInputData vxgiData;

layout(binding = 0) uniform sampler2D t_DiffuseColor;
layout(binding = 1) uniform sampler2DShadow t_ShadowMap;

const float PI = 3.14159265;

float GetShadowFast(vec3 fragmentPos)
{
    vec4 clipPos = vec4(fragmentPos, 1.0) * g_LightViewProjMatrix;

    // Early out
    if (abs(clipPos.x) > clipPos.w || abs(clipPos.y) > clipPos.w || abs(clipPos.z) > clipPos.w)
    {
        return 0;
    }

    clipPos.xyz /= clipPos.w;
    clipPos.xyz = clipPos.xyz * 0.5 + 0.5;

    return texture(t_ShadowMap, clipPos.xyz);
}

void main()
{
    if(bool(VxgiIsEmissiveVoxelizationPass))
    {
        vec3 worldPos = v_positionWS.xyz;
        vec3 normal = normalize(v_normal.xyz);


        vec3 albedo = g_DiffuseColor.rgb;

        if(g_DiffuseColor.a > 0)
            albedo = texture(t_DiffuseColor, v_texCoord.xy).rgb;

        float NdotL = clamp(-dot(normal, g_LightDirection.xyz), 0.0, 1.0);

        vec3 radiosity = vec3(0);

        if(NdotL > 0)
        {
            float shadow = GetShadowFast(worldPos);

            radiosity += albedo.rgb * g_LightColor.rgb * (NdotL * shadow);
        }

        radiosity += albedo.rgb * VxgiGetIndirectIrradiance(worldPos, normal) / PI;

        VxgiStoreVoxelizationData(vxgiData, radiosity);
    }
    else
    {
        VxgiStoreVoxelizationData(vxgiData, vec3(0));
    }
})";
