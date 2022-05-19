/* 
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved. 
* 
* NVIDIA CORPORATION and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto. Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an express 
* license agreement from NVIDIA CORPORATION is strictly prohibited. 
*/ 

char const g_VoxelizationPS[] = R"(
Texture2D g_normal_texture : register(t3);
Texture2D g_base_color_texture : register(t4);
Texture2D g_roughness_metallic_texture : register(t5);

SamplerState g_sampler : register(s0);

void main(
    in float4 position : SV_Position,
    in float3 in_interpolated_position_world_space : LOCATION0,
    in float3 in_interpolated_normal : LOCATION1,
    in float4 in_interpolated_tangent : LOCATION2,
    in float2 in_interpolated_texcoord : LOCATION3,
    in VxgiVoxelizationPSInputData in_vxgi_data
    )
{
    float3 shading_normal_world_space;
    float3 diffuse_color;
    float3 specular_color;
    float opacity;
    float roughness;
    {
        {
            float3 geometry_normal_world_space = normalize(in_interpolated_normal);

            float3 tangent_world_space = normalize(in_interpolated_tangent.xyz);

            float3 bitangent_world_space = cross(geometry_normal_world_space, tangent_world_space) * ((in_interpolated_tangent.w >= 0.0) ? 1.0 : -1.0);

            float3 shading_normal_tangent_space = normalize(g_normal_texture.Sample(g_sampler, in_interpolated_texcoord).xyz * 2.0 - float3(1.0, 1.0, 1.0));

            shading_normal_world_space = normalize(tangent_world_space * shading_normal_tangent_space.x + bitangent_world_space * shading_normal_tangent_space.y + geometry_normal_world_space * shading_normal_tangent_space.z);
        }

        {
            float4 base_color_and_opacity = g_base_color_texture.Sample(g_sampler, in_interpolated_texcoord);

            float3 base_color = base_color_and_opacity.xyz;

            opacity = base_color_and_opacity.w;

            float2 roughness_metallic = g_roughness_metallic_texture.Sample(g_sampler, in_interpolated_texcoord).yz;

            roughness = roughness_metallic.x;

            float metallic = roughness_metallic.y;

            // UE4: https://github.com/EpicGames/UnrealEngine/blob/4.21/Engine/Shaders/Private/MobileBasePassPixelShader.usf#L376
            const float dielectric_specular = 0.04;

            specular_color = clamp((dielectric_specular - dielectric_specular * metallic) + base_color * metallic, 0.0, 1.0);
            diffuse_color = clamp(base_color - base_color * metallic, 0.0, 1.0);
        }
    }

    [branch]
    if (opacity < 0.5)
    {
        discard;
        return;
    }

    VxgiStoreVoxelizationData(in_vxgi_data, float3(0.0, 0.0, 0.0), 1.0, float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 0.0));
})";
