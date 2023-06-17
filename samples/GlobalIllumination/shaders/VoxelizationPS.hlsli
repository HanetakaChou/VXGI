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
    float2 texCoord : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
    float3 positionWS : WSPOSITION;
    VxgiVoxelizationPSInputData vxgiData;
};

cbuffer GlobalConstants : register(b0)
{
    float4x4 g_ViewProjMatrix;
    float4x4 g_ViewProjMatrixInv;
    float4x4 g_LightViewProjMatrix;
    float4 g_LightDirection;
    float4 g_DiffuseColor;
    float4 g_LightColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
};

Texture2D<float4> t_DiffuseColor    : register(t0);
Texture2D t_ShadowMap               : register(t1);
SamplerState g_SamplerLinearWrap    : register(s0);
SamplerComparisonState g_SamplerComparison : register(s1);

static const float PI = 3.14159265;

float GetShadowFast(float3 fragmentPos)
{
    float4 clipPos = mul(float4(fragmentPos, 1.0f), g_LightViewProjMatrix);

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


        float3 albedo = g_DiffuseColor.rgb;

        if(g_DiffuseColor.a > 0)
            albedo = t_DiffuseColor.Sample(g_SamplerLinearWrap, IN.texCoord.xy).rgb;

        float NdotL = saturate(-dot(normal, g_LightDirection.xyz));

        float3 radiosity = 0;

        if(NdotL > 0)
        {
            float shadow = GetShadowFast(worldPos);

            radiosity += albedo.rgb * g_LightColor.rgb * (NdotL * shadow);
        }

        radiosity += albedo.rgb * VxgiGetIndirectIrradiance(worldPos, normal) / PI;

        VxgiStoreVoxelizationData(IN.vxgiData, radiosity);
    }
    else
    {
        VxgiStoreVoxelizationData(IN.vxgiData, 0);
    }
})";
