/* 
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved. 
* 
* NVIDIA CORPORATION and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto. Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an express 
* license agreement from NVIDIA CORPORATION is strictly prohibited. 
*/ 

char const g_VoxelizationPS[] = R"(struct PSInput 
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float3 positionWS : WSPOSITION;
    VxgiVoxelizationPSInputData vxgiData;
};

cbuffer GlobalConstants : register(b0)
{
    float4x4 g_ViewProjMatrix;
    float4x4 g_ViewProjMatrixInv;
    float4x4 g_LightViewProjMatrix;
    float4 g_LightPos;
    float4 g_LightColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
};

cbuffer MaterialConstants : register(b1)
{
    float4 g_BaseColor;
    float g_Metallic;
    float g_Roughness;
};

Texture2D t_ShadowMap               : register(t1);
SamplerState g_SamplerLinearWrap    : register(s0);
SamplerComparisonState g_SamplerComparison : register(s1);

static const float PI = 3.14159265;

float GetShadowFast(float3 worldPos)
{
    float4 clipPos = mul(float4(worldPos, 1.0f), g_LightViewProjMatrix);

    // Early out
    if (abs(clipPos.x) > clipPos.w || abs(clipPos.y) > clipPos.w || clipPos.z <= 0)
    {
        return 0;
    }

    clipPos.xyz /= clipPos.w;
    clipPos.x = clipPos.x * 0.5f + 0.5f;
    clipPos.y = 0.5f - clipPos.y * 0.5f;

    return t_ShadowMap.SampleCmpLevelZero(g_SamplerComparison, clipPos.xy, clipPos.z);
}

void main(PSInput IN)
{
    if(VxgiIsEmissiveVoxelizationPass)
    {
        float3 worldPos = IN.positionWS.xyz;
        float3 normal = normalize(IN.normal.xyz);

        // \[Bhatia 2017\] [Saurabh Bhatia. "glTF 2.0: PBR Materials." GTC 2017.](https://www.khronos.org/assets/uploads/developers/library/2017-gtc/glTF-2.0-and-PBR-GTC_May17.pdf)
        float3 specular_color_dielectric = float3(0.04, 0.04, 0.04);
        float3 specular_color = lerp(specular_color_dielectric, g_BaseColor.xyz, g_Metallic);
        float3 diffuse_color = g_BaseColor.xyz - specular_color;

        float3 radiance = float3(0.0, 0.0, 0.0);

        float3 light_direction = normalize(g_LightPos.xyz - worldPos);
        float NdotL = dot(normal, light_direction);
        [branch]
        if(NdotL > 0.0)
        {
            float shadow = GetShadowFast(worldPos);
            radiance += diffuse_color * g_LightColor.rgb * (NdotL * shadow);
        }

        radiance += diffuse_color * VxgiGetIndirectIrradiance(worldPos, normal) / PI;

        VxgiStoreVoxelizationData(IN.vxgiData, radiance);
    }
    else
    {
        VxgiStoreVoxelizationData(IN.vxgiData, 0);
    }
})";
