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
cbuffer GlobalConstants : register(b0)
{
    float4x4 g_WorldMatrix;
    float4x4 g_ViewProjMatrix;
    float4x4 g_ViewProjMatrixInv;
    float4x4 g_LightViewProjMatrix;
    float4x4 g_viewport_depth_direction_view_matrices[3];
    float4x4 g_clipmap_stack_level_projection_matrices[5];
    float4 g_clipmap_center;
    float4 g_CameraPos;
    float4 g_LightDirection;
    float4 g_DiffuseColor;
    float4 g_LightColor;
    float4 g_AmbientColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
    float g_TransparentRoughness;
    float g_TransparentReflectance;
}

Texture2D g_normal_texture : register(t3);
Texture2D g_base_color_texture : register(t4);
Texture2D g_roughness_metallic_texture : register(t5);
Texture2D g_shadow_map : register(t6);

SamplerState g_sampler : register(s0);
SamplerComparisonState g_shadow_sampler : register(s1);

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

    return g_shadow_map.SampleCmpLevelZero(g_shadow_sampler, clipPos.xy, clipPos.z);
}

float GetShadow(float3 fragmentPos)
{
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
        float2(0.08265103f, -0.8939569f)};

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

    for (int nSample = 0; nSample < 16; ++nSample)
    {
        float2 offset = g_SamplePositions[nSample];
        float weight = 1.0;
        offset *= 2 * g_rShadowMapSize;
        float sample = g_shadow_map.SampleCmpLevelZero(g_shadow_sampler, clipPos.xy + offset, clipPos.z);
        shadow += sample * weight;
        totalWeight += weight;
    }

    shadow /= totalWeight;
    shadow = pow(shadow, 2.2);

    return shadow;
}

void main(
    in float4 position : SV_Position,
    in float3 in_interpolated_position_world_space : LOCATION0,
    in float3 in_interpolated_normal : LOCATION1,
    in float4 in_interpolated_tangent : LOCATION2,
    in float2 in_interpolated_texcoord : LOCATION3,
    in VxgiVoxelizationPSInputData in_vxgi_data)
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

    float3 surface_position_world_space = in_interpolated_position_world_space;

    float3 E_n;
    {
        float3 L = normalize(-g_LightDirection.xyz);
        float3 N = shading_normal_world_space;

        float NdotL = dot(N, L);

        float3 E_l;
        [branch] if (NdotL > 0.0)
        {
            float shadow = GetShadow(surface_position_world_space);

            E_l = g_LightColor.rgb * shadow;
        }
        else
        {
            E_l = float3(0.0, 0.0, 0.0);
        }

        E_n = E_l * NdotL;
    }

    // photon mapping
    //
    // \Delta\Phi = E_n * Area / N_p = E_l * cos_theta * Area / N_p
    //
    // L = f * \Delta\Phi = f * E_l * cos_theta * Area / N_p
    //
    // we calculate "f * E_l * cos_theta" here
    //
    // the outgoing direction of BRDF should be the cone direction
    // we do not have such information here and we only calculate diffuse without specular
    //
    float3 brdf = (1.0 / PI) * diffuse_color;
    float3 DeltaPhi_div_area = E_n;
    float3 brdf_mul_DeltaPhi_div_area = brdf * DeltaPhi_div_area;
    VxgiStoreVoxelizationData(in_vxgi_data, shading_normal_world_space, 1.0, brdf_mul_DeltaPhi_div_area, 0);
}
)";
