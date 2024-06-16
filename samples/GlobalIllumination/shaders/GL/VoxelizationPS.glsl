char const g_VoxelizationPS[] = R"(layout(row_major) uniform;

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

layout(std140, binding = 1) uniform MaterialConstants
{
    vec4 g_BaseColor;
    float g_Metallic;
    float g_Roughness;
};

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_positionWS;

flat in VxgiVoxelizationPSInputData vxgiData;

layout(binding = 1) uniform sampler2DShadow t_ShadowMap;

const float PI = 3.14159265;

float GetShadowFast(vec3 worldPos)
{
    vec4 clipPos = vec4(worldPos, 1.0) * g_LightViewProjMatrix;

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

        // \[Bhatia 2017\] [Saurabh Bhatia. "glTF 2.0: PBR Materials." GTC 2017.](https://www.khronos.org/assets/uploads/developers/library/2017-gtc/glTF-2.0-and-PBR-GTC_May17.pdf)
        vec3 specular_color_dielectric = vec3(0.04, 0.04, 0.04);
        vec3 specular_color = lerp(specular_color_dielectric, g_BaseColor.xyz, g_Metallic);
        vec3 diffuse_color = g_BaseColor.xyz - specular_color;

        vec3 radiance = vec3(0.0, 0.0, 0.0);

        vec3 light_direction = normalize(g_LightPos.xyz - worldPos);
		float NdotL = dot(normal, light_direction);
        if(NdotL > 0.0)
        {
            float shadow = GetShadowFast(worldPos);
            radiance += diffuse_color * g_LightColor.rgb * (NdotL * shadow);
        }

        radiance += diffuse_color * VxgiGetIndirectIrradiance(worldPos, normal) / PI;

        VxgiStoreVoxelizationData(vxgiData, radiance);
    }
    else
    {
        VxgiStoreVoxelizationData(vxgiData, vec3(0));
    }
})";
