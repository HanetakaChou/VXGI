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

Texture2D DiffuseTexture    : register(t0);
Texture2D SpecularTexture   : register(t1);
Texture2D NormalTexture     : register(t2);

SamplerState DefaultSampler : register(s0);

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

float4 VoxelizationVS(float3 position: POSITION): SV_Position
{
    return mul(float4(position.xyz, 1.0f), g_ViewProjMatrix);
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
    float3 pixelNormal = NormalTexture.Sample(DefaultSampler, input.texCoord).xyz;

    float3 normal = normalize(input.normal);

    if(pixelNormal.z)
    {
        float3 tangent = normalize(input.tangent);
        float3 binormal = normalize(input.binormal);
        float3x3 TangentMatrix = float3x3(tangent, binormal, normal);
        normal = normalize(mul(pixelNormal * 2 - 1, TangentMatrix));
    }

    output.albedo = diffuseColor;
    output.normal = float4(normal, 0);

    return output;
}

struct FullScreenQuadOutput
{
    float4 position     : SV_Position;
    float2 uv           : TEXCOORD;
};

FullScreenQuadOutput FullScreenQuadVS(uint id : SV_VertexID)
{
    FullScreenQuadOutput OUT;

    uint u = ~id & 1;
    uint v = (id >> 1) & 1;
    OUT.uv = float2(u, v);
    OUT.position = float4(OUT.uv * 2 - 1, 0, 1);

    // In D3D (0, 0) stands for upper left corner
    OUT.uv.y = 1.0 - OUT.uv.y;

    return OUT;
}

Texture2D t_SourceTexture : register(t0);

float4 BlitPS(FullScreenQuadOutput IN): SV_Target
{
    return t_SourceTexture[IN.position.xy];
}

Texture2D t_AmbientOcclusion : register(t0);
Texture2D t_GBufferAlbedo : register(t1);

float4 CompositingPS(FullScreenQuadOutput IN): SV_Target
{
    float4 ao = t_AmbientOcclusion[IN.position.xy];
    float4 albedo = t_GBufferAlbedo[IN.position.xy];
    return ao * albedo;
}