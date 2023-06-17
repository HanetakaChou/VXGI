/* 
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved. 
* 
* NVIDIA CORPORATION and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto. Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an express 
* license agreement from NVIDIA CORPORATION is strictly prohibited. 
*/ 

#pragma pack_matrix( row_major )

cbuffer GlobalConstants : register(b0)
{
    float4x4 g_ViewProjMatrix;
    float4x4 g_ViewProjMatrixInv;
    float4x4 g_LightViewProjMatrix;
    float4 g_LightDirection;
    float4 g_DiffuseColor;
    float4 g_LightColor;
    float4 g_AmbientColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
}

struct PS_Input
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
    float3 positionWS : WSPOSITION;
};

static const float2 g_SamplePositions[] = {
    // Poisson disk with 16 points
    float2(-0.3935238f, 0.7530643f),
    float2(-0.3022015f, 0.297664f),
    float2(0.09813362f, 0.192451f),
    float2(-0.7593753f, 0.518795f),
    float2(0.2293134f, 0.7607011f),
    float2(0.6505286f, 0.6297367f),
    float2(0.5322764f, 0.2350069f),
    float2(0.8581018f, -0.01624052f),
    float2(-0.6928226f, 0.07119545f),
    float2(-0.3114384f, -0.3017288f),
    float2(0.2837671f, -0.179743f),
    float2(-0.3093514f, -0.749256f),
    float2(-0.7386893f, -0.5215692f),
    float2(0.3988827f, -0.617012f),
    float2(0.8114883f, -0.458026f),
    float2(0.08265103f, -0.8939569f)
};

Texture2D DiffuseTexture    : register(t0);
Texture2D SpecularTexture   : register(t1);
Texture2D NormalTexture     : register(t2);

SamplerState DefaultSampler : register(s0);

#define USE_FLAT_NORMALS 0

struct VS_Input
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
};

PS_Input DefaultVS(VS_Input input)
{
    PS_Input output;

    output.position = mul(float4(input.position.xyz, 1.0f), g_ViewProjMatrix);
    output.positionWS = input.position.xyz;

    output.texCoord = input.texCoord;
    output.normal = input.normal;
    output.tangent = input.tangent;
    output.binormal = input.binormal;

    return output;
} 

float3 GetNormal(PS_Input input)
{
#if USE_FLAT_NORMALS
    float3 normal = normalize(cross(ddx(input.positionWS.xyz), ddy(input.positionWS.xyz)));
#else
    float3 pixelNormal = NormalTexture.Sample(DefaultSampler, input.texCoord).xyz;
    float3 normal = normalize(input.normal);

    if(pixelNormal.z)
    {
        float3 tangent = normalize(input.tangent);
        float3 binormal = normalize(input.binormal);
        float3x3 TangentMatrix = float3x3(tangent, binormal, normal);
        normal = normalize(mul(pixelNormal * 2 - 1, TangentMatrix));
    }
#endif

    return normal;
}

struct PS_Attributes
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
};

PS_Attributes AttributesPS(PS_Input input)
{
    PS_Attributes output;

    float4 diffuseColor;
    diffuseColor.rgb = DiffuseTexture.Sample(DefaultSampler, input.texCoord).rgb;
    diffuseColor.a = SpecularTexture.Sample(DefaultSampler, input.texCoord).r;
    float3 normal = GetNormal(input);
    float roughness = (diffuseColor.a > 0) ? 0.5 : 0;

    output.albedo = diffuseColor;
    output.normal = float4(normal.xyz, roughness);

    return output;
}


struct FullScreenQuadOutput
{
    float4 position     : SV_Position;
    float2 clipSpacePos : CLIP;
};

FullScreenQuadOutput FullScreenQuadVS(uint id : SV_VertexID)
{
    FullScreenQuadOutput OUT;

    uint u = ~id & 1;
    uint v = (id >> 1) & 1;
    OUT.position = float4(float2(u,v) * 2 - 1, 0, 1);
    OUT.clipSpacePos = OUT.position.xy;

    return OUT;
}


Texture2D t_SourceTexture : register(t0);

float4 BlitPS(FullScreenQuadOutput IN): SV_Target
{
    return t_SourceTexture[IN.position.xy];
}


Texture2D t_GBufferAlbedo     : register(t0);
Texture2D t_GBufferNormal     : register(t1);
Texture2D t_GBufferDepth      : register(t2);
Texture2D t_IndirectDiffuse   : register(t3);
Texture2D t_IndirectSpecular  : register(t4);
Texture2D t_ShadowMap         : register(t5);

SamplerComparisonState ShadowSampler : register(s0);

float GetShadow(float3 fragmentPos)
{
    fragmentPos -= g_LightDirection.xyz * 1.0f;

    float4 clipPos = mul(float4(fragmentPos, 1.0f), g_LightViewProjMatrix);

    if (abs(clipPos.x) > clipPos.w || abs(clipPos.y) > clipPos.w || clipPos.z <= 0)
    {
        return 0;
    }

    clipPos.xyz /= clipPos.w;
    clipPos.x = clipPos.x * 0.5f + 0.5f;
    clipPos.y = 0.5f - clipPos.y * 0.5f;

    float shadow = 0;
    float totalWeight = 0;

    for(int nSample = 0; nSample < 16; ++nSample)
    {
        float2 offset = g_SamplePositions[nSample];
        float weight = 1.0;
        offset *= 2 * g_rShadowMapSize;
        float sample = t_ShadowMap.SampleCmpLevelZero(ShadowSampler, clipPos.xy + offset, clipPos.z);
        shadow += sample * weight;
        totalWeight += weight;
    }

    shadow /= totalWeight;
    shadow = pow(shadow, 2.2);

    return shadow;
}

float Luminance(float3 color)
{
    return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
}

float3 ConvertToLDR(float3 color)
{
    float srcLuminance = Luminance(color);

    float sqrWhiteLuminance = 50;
    float scaledLuminance = srcLuminance * 8;
    float mappedLuminance = (scaledLuminance * (1 + scaledLuminance / sqrWhiteLuminance)) / (1 + scaledLuminance);

    return color * (mappedLuminance / srcLuminance);
}

float4 CompositingPS(FullScreenQuadOutput IN): SV_Target
{
    float4 albedo = t_GBufferAlbedo[IN.position.xy];
    float3 normal = t_GBufferNormal[IN.position.xy].xyz;
    float4 indirectDiffuse = g_EnableIndirectDiffuse ? t_IndirectDiffuse[IN.position.xy] : 0;
    float3 indirectSpecular = g_EnableIndirectSpecular ? t_IndirectSpecular[IN.position.xy].rgb : 0;
    float z = t_GBufferDepth[IN.position.xy].x;

    float4 worldPos = mul(float4(IN.clipSpacePos.xy, z, 1), g_ViewProjMatrixInv);
    worldPos.xyz /= worldPos.w;

    float shadow = GetShadow(worldPos.xyz);
    float NdotL = saturate(-dot(normal.xyz, g_LightDirection.xyz));

    float3 result = albedo.rgb * (g_LightColor.rgb * (shadow * NdotL) + lerp(g_AmbientColor.rgb, indirectDiffuse.rgb, indirectDiffuse.a)) 
                  + albedo.a * indirectSpecular.rgb;

    result = ConvertToLDR(result);

    return float4(result, 1);
}