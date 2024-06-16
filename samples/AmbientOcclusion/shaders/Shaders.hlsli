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

cbuffer MaterialConstants : register(b1)
{
    float4 g_BaseColor;
    float g_Metallic;
    float g_Roughness;
};

struct PS_Input
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float3 positionWS : WSPOSITION;
};

SamplerState DefaultSampler : register(s0);

struct VS_Input
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

PS_Input DefaultVS(VS_Input input)
{
    PS_Input output;

    float4 worldPos = float4(input.position.xyz, 1.0f);
    output.position = mul(worldPos, g_ViewProjMatrix);
    output.positionWS = worldPos.xyz;

    output.normal = input.normal;

    return output;
} 

float4 VoxelizationVS(float3 position: POSITION): SV_Position
{
    return mul(float4(position.xyz, 1.0f), g_ViewProjMatrix);
}

struct PS_Attributes
{
    float4 GBufferA : SV_Target1;
    float4 GBufferC : SV_Target0;
};

PS_Attributes AttributesPS(PS_Input input)
{
    float3 normal = normalize(input.normal);
    float roughness = g_Roughness;
    float3 base_color = g_BaseColor.xyz;
    float metallic = g_Metallic;

    PS_Attributes output;
    output.GBufferA = float4(normal, roughness);
    output.GBufferC = float4(base_color, metallic);

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

Texture2D t_VXAO : register(t0);
Texture2D t_GBufferGBufferC : register(t1);

float4 CompositingPS(FullScreenQuadOutput IN): SV_Target
{
    float vxao = t_VXAO[IN.position.xy].x;
    float ao = vxao;

    float4 GBufferC = t_GBufferGBufferC[IN.position.xy];
    float3 base_color = GBufferC.xyz;

    return float4(base_color * ao, 1.0);
}