#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_shader_language.bsli"
#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_packed_vector.bsli"
#include "../../../thirdparty/Environment-Lighting/shaders/brx_octahedral_mapping.bsli"
#include "../../../thirdparty/Voxel-Cone-Tracing/shaders/brx_voxel_cone_tracing_voxelization_vertex.bsli"

#pragma pack_matrix(row_major)

#define SURFACE_VERTEX_POSITION_BUFFER_STRIDE 12u
#define SURFACE_VERTEX_VARYING_BUFFER_STRIDE 12u
#define SURFACE_UINT32_INDEX_BUFFER_STRIDE 4u

#include "../GlobalConstants.h"

ByteAddressBuffer g_vertex_position_buffer : register(t0);
ByteAddressBuffer g_vertex_varying_buffer : register(t1);
ByteAddressBuffer g_index_buffer : register(t2);
Texture2D g_normal_texture : register(t3);
Texture2D g_base_color_texture : register(t4);
Texture2D g_roughness_metallic_texture : register(t5);

SamplerState g_sampler : register(s0);

void DefaultVS(
    in uint vertex_id : SV_VertexID,
    out float4 out_position : SV_Position,
    out float3 out_vertex_position_world_space : LOCATION0,
    out float3 out_vertex_normal : LOCATION1,
    out float4 out_vertex_tangent : LOCATION2,
    out float2 out_vertex_texcoord : LOCATION3)
{
    brx_uint vertex_index;
    {
        brx_uint index_buffer_offset = SURFACE_UINT32_INDEX_BUFFER_STRIDE * brx_uint(vertex_id);
        vertex_index = brx_byte_address_buffer_load(g_index_buffer, index_buffer_offset);
    }

    brx_float3 vertex_position_model_space;
    {
        brx_uint vertex_position_buffer_offset = SURFACE_VERTEX_POSITION_BUFFER_STRIDE * vertex_index;
        brx_uint3 packed_vector_vertex_position_binding = brx_byte_address_buffer_load3(g_vertex_position_buffer, vertex_position_buffer_offset);
        vertex_position_model_space = brx_uint_as_float(packed_vector_vertex_position_binding);
    }

    brx_float3 vertex_normal_model_space;
    brx_float4 vertex_tangent_model_space;
    brx_float2 vertex_texcoord;
    {
        brx_uint vertex_varying_buffer_offset = SURFACE_VERTEX_VARYING_BUFFER_STRIDE * vertex_index;
        brx_uint3 packed_vector_vertex_varying_binding = brx_byte_address_buffer_load3(g_vertex_varying_buffer, vertex_varying_buffer_offset);
        vertex_normal_model_space = brx_octahedral_unmap(brx_R16G16_SNORM_to_FLOAT2(packed_vector_vertex_varying_binding.x));
        brx_float3 vertex_mapped_tangent_model_space = brx_R15G15B2_SNORM_to_FLOAT3(packed_vector_vertex_varying_binding.y);
        vertex_tangent_model_space = brx_float4(brx_octahedral_unmap(vertex_mapped_tangent_model_space.xy), vertex_mapped_tangent_model_space.z);
        // https://github.com/AcademySoftwareFoundation/Imath/blob/main/src/Imath/half.h
        // HALF_MAX 65504.0
        vertex_texcoord = brx_clamp(brx_unpack_half2(packed_vector_vertex_varying_binding.z), brx_float2(-65504.0, -65504.0), brx_float2(65504.0, 65504.0));
    }

    float3 vertex_position_world_space = mul(float4(vertex_position_model_space, 1.0), g_WorldMatrix).xyz;
    float4 vertex_position_clip_space = mul(float4(vertex_position_world_space, 1.0f), g_ViewProjMatrix);

    float3 vertex_normal_world_space = mul(float4(vertex_normal_model_space, 0.0), g_WorldMatrix).xyz;
    float4 vertex_tangent_world_space = float4(mul(float4(vertex_tangent_model_space.xyz, 0.0), g_WorldMatrix).xyz, vertex_tangent_model_space.w);

    out_position = vertex_position_clip_space;
    out_vertex_position_world_space = vertex_position_world_space;
    out_vertex_normal = vertex_normal_world_space;
    out_vertex_tangent = vertex_tangent_world_space;
    out_vertex_texcoord = vertex_texcoord;
}

void MyVoxelizationVS(
    in uint in_vertex_id : SV_VertexID,
    in uint in_instance_id : SV_InstanceID,
    out float4 out_position : SV_Position,
    out float out_cull_distance[2] : SV_CullDistance,
    out nointerpolation int out_viewport_depth_direction_index : LOCATION0,
    out nointerpolation int out_clipmap_stack_level_index : LOCATION1,
    out float3 out_vertex_position_world_space : LOCATION2,
    out float3 out_vertex_normal : LOCATION3,
    out float4 out_vertex_tangent : LOCATION4,
    out float2 out_vertex_texcoord : LOCATION5)
{

    brx_uint3 triangle_vertex_indices;
    {
        brx_uint triangle_index = in_vertex_id / 3u;
        brx_uint index_buffer_offset = (SURFACE_UINT32_INDEX_BUFFER_STRIDE * 3u) * triangle_index;
        triangle_vertex_indices = brx_byte_address_buffer_load3(g_index_buffer, index_buffer_offset);
    }

    brx_float3 triangle_vertices_position_model_space[3];
    {
        brx_uint3 vertex_position_buffer_offset = SURFACE_VERTEX_POSITION_BUFFER_STRIDE * triangle_vertex_indices;

        brx_uint3 packed_vector_vertices_position_binding[3];
        packed_vector_vertices_position_binding[0] = brx_byte_address_buffer_load3(g_vertex_position_buffer, vertex_position_buffer_offset.x);
        packed_vector_vertices_position_binding[1] = brx_byte_address_buffer_load3(g_vertex_position_buffer, vertex_position_buffer_offset.y);
        packed_vector_vertices_position_binding[2] = brx_byte_address_buffer_load3(g_vertex_position_buffer, vertex_position_buffer_offset.z);

        triangle_vertices_position_model_space[0] = brx_uint_as_float(packed_vector_vertices_position_binding[0]);
        triangle_vertices_position_model_space[1] = brx_uint_as_float(packed_vector_vertices_position_binding[1]);
        triangle_vertices_position_model_space[2] = brx_uint_as_float(packed_vector_vertices_position_binding[2]);
    }

    brx_float3 triangle_vertices_position_world_space[3];
    {
        triangle_vertices_position_world_space[0] = mul(float4(triangle_vertices_position_model_space[0], 1.0), g_WorldMatrix).xyz;
        triangle_vertices_position_world_space[1] = mul(float4(triangle_vertices_position_model_space[1], 1.0), g_WorldMatrix).xyz;
        triangle_vertices_position_world_space[2] = mul(float4(triangle_vertices_position_model_space[2], 1.0), g_WorldMatrix).xyz;
    }

    brx_int viewport_depth_direction_index = brx_voxel_cone_tracing_voxelization_compute_viewport_depth_direction_index(triangle_vertices_position_world_space[0], triangle_vertices_position_world_space[1], triangle_vertices_position_world_space[2]);

    brx_int clipmap_stack_level_index = in_instance_id;

    brx_uint vertex_index;
    {
        brx_uint index_buffer_offset = SURFACE_UINT32_INDEX_BUFFER_STRIDE * brx_uint(in_vertex_id);
        vertex_index = brx_byte_address_buffer_load(g_index_buffer, index_buffer_offset);
    }

    brx_float3 vertex_position_model_space;
    {
        brx_uint vertex_position_buffer_offset = SURFACE_VERTEX_POSITION_BUFFER_STRIDE * vertex_index;
        brx_uint3 packed_vector_vertex_position_binding = brx_byte_address_buffer_load3(g_vertex_position_buffer, vertex_position_buffer_offset);
        vertex_position_model_space = brx_uint_as_float(packed_vector_vertex_position_binding);
    }

    brx_float3 vertex_normal_model_space;
    brx_float4 vertex_tangent_model_space;
    brx_float2 vertex_texcoord;
    {
        brx_uint vertex_varying_buffer_offset = SURFACE_VERTEX_VARYING_BUFFER_STRIDE * vertex_index;
        brx_uint3 packed_vector_vertex_varying_binding = brx_byte_address_buffer_load3(g_vertex_varying_buffer, vertex_varying_buffer_offset);
        vertex_normal_model_space = brx_octahedral_unmap(brx_R16G16_SNORM_to_FLOAT2(packed_vector_vertex_varying_binding.x));
        brx_float3 vertex_mapped_tangent_model_space = brx_R15G15B2_SNORM_to_FLOAT3(packed_vector_vertex_varying_binding.y);
        vertex_tangent_model_space = brx_float4(brx_octahedral_unmap(vertex_mapped_tangent_model_space.xy), vertex_mapped_tangent_model_space.z);
        // https://github.com/AcademySoftwareFoundation/Imath/blob/main/src/Imath/half.h
        // HALF_MAX 65504.0
        vertex_texcoord = brx_clamp(brx_unpack_half2(packed_vector_vertex_varying_binding.z), brx_float2(-65504.0, -65504.0), brx_float2(65504.0, 65504.0));
    }

    brx_float3 vertex_position_world_space = mul(float4(vertex_position_model_space, 1.0), g_WorldMatrix).xyz;
    brx_float3 vertex_position_view_space = mul(float4(vertex_position_world_space, 1.0), g_viewport_depth_direction_view_matrices[viewport_depth_direction_index]).xyz;
    brx_float4 vertex_position_clip_space = mul(float4(vertex_position_view_space, 1.0), g_clipmap_stack_level_projection_matrices[clipmap_stack_level_index]);

    brx_float2 cull_distance = brx_voxel_cone_tracing_voxelization_compute_cull_distance(vertex_position_clip_space);

    brx_float3 vertex_normal_world_space = mul(float4(vertex_normal_model_space, 0.0), g_WorldMatrix).xyz;
    brx_float4 vertex_tangent_world_space = float4(mul(float4(vertex_tangent_model_space.xyz, 0.0), g_WorldMatrix).xyz, vertex_tangent_model_space.w);

    out_position = vertex_position_clip_space;
    out_cull_distance[0] = cull_distance.x;
    out_cull_distance[1] = cull_distance.y;
    out_viewport_depth_direction_index = viewport_depth_direction_index;
    out_clipmap_stack_level_index = clipmap_stack_level_index;
    out_vertex_position_world_space = vertex_position_world_space;
    out_vertex_normal = vertex_normal_world_space;
    out_vertex_tangent = vertex_tangent_world_space;
    out_vertex_texcoord = vertex_texcoord;
}

void AttributesPS(
    in float4 position : SV_Position,
    in float3 in_interpolated_position_world_space : LOCATION0,
    in float3 in_interpolated_normal : LOCATION1,
    in float4 in_interpolated_tangent : LOCATION2,
    in float2 in_interpolated_texcoord : LOCATION3,
    out float4 out_base_color_and_metallic : SV_Target0,
    out float4 out_normal_and_roughness : SV_Target1)
{
    float3 shading_normal_world_space;
    float3 base_color;
    float opacity;
    float roughness;
    float metallic;
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

            base_color = base_color_and_opacity.xyz;

            opacity = base_color_and_opacity.w;
        }

        {
            float2 roughness_metallic = g_roughness_metallic_texture.Sample(g_sampler, in_interpolated_texcoord).yz;

            roughness = roughness_metallic.x;

            metallic = roughness_metallic.y;
        }
    }

    if (opacity < 0.5)
    {
        discard;
    }

    out_base_color_and_metallic = float4(base_color, metallic);
    out_normal_and_roughness = float4(shading_normal_world_space, roughness);
}

struct FullScreenQuadOutput
{
    float4 position : SV_Position;
    float2 clipSpacePos : CLIP;
};

FullScreenQuadOutput FullScreenQuadVS(uint id : SV_VertexID)
{
    FullScreenQuadOutput OUT;

    uint u = ~id & 1;
    uint v = (id >> 1) & 1;
    OUT.position = float4(float2(u, v) * 2 - 1, 0, 1);
    OUT.clipSpacePos = OUT.position.xy;

    return OUT;
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

Texture2D t_SourceTexture : register(t0);

float4 BlitPS(FullScreenQuadOutput IN) : SV_Target
{
    return t_SourceTexture[IN.position.xy];
}

float4 BlitLdrPS(FullScreenQuadOutput IN) : SV_Target
{
    float3 color = ConvertToLDR(t_SourceTexture[IN.position.xy].rgb);
    return float4(color, 1);
}

Texture2D g_gbuffer_base_color_and_metallic : register(t0);
Texture2D g_gbuffer_normal_and_roughness : register(t1);
Texture2D t_GBufferDepth : register(t2);
Texture2D t_IndirectDiffuse : register(t3);
Texture2D t_IndirectSpecular : register(t4);
Texture2D t_IndirectConfidence : register(t5);
Texture2D g_ShadowMap : register(t6);

SamplerComparisonState g_shadow_sampler : register(s0);

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
        float sample = g_ShadowMap.SampleCmpLevelZero(g_shadow_sampler, clipPos.xy + offset, clipPos.z);
        shadow += sample * weight;
        totalWeight += weight;
    }

    shadow /= totalWeight;
    shadow = pow(shadow, 2.2);

    return shadow;
}

float4 CompositingPS(FullScreenQuadOutput IN) : SV_Target
{
    float3 shading_normal_world_space;
    float3 diffuse_color;
    float3 specular_color;
    float roughness;
    {
        float4 base_color_and_metallic = g_gbuffer_base_color_and_metallic[IN.position.xy];
        float4 normal_and_roughness = g_gbuffer_normal_and_roughness[IN.position.xy];

        shading_normal_world_space = normal_and_roughness.xyz;
        roughness = normal_and_roughness.w;

        float3 base_color = base_color_and_metallic.xyz;
        float metallic = base_color_and_metallic.w;

        // UE4: https://github.com/EpicGames/UnrealEngine/blob/4.21/Engine/Shaders/Private/MobileBasePassPixelShader.usf#L376
        const float dielectric_specular = 0.04;

        specular_color = clamp((dielectric_specular - dielectric_specular * metallic) + base_color * metallic, 0.0, 1.0);
        diffuse_color = clamp(base_color - base_color * metallic, 0.0, 1.0);
    }

    brx_float3 surface_position_world_space;
    {
        float z = t_GBufferDepth[IN.position.xy].x;
        float4 worldPos = mul(float4(IN.clipSpacePos.xy, z, 1), g_ViewProjMatrixInv);
        worldPos.xyz /= worldPos.w;

        surface_position_world_space = worldPos.xyz;
    }

    brx_float3 camera_ray_direction;
    {
        brx_float3 camera_ray_origin = g_CameraPos.xyz; // brx_mul(g_inverse_view_transform, brx_float4(0.0, 0.0, 0.0, 1.0)).xyz;
        camera_ray_direction = brx_normalize(surface_position_world_space - camera_ray_origin);
    }

    float3 radiance = float3(0.0, 0.0, 0.0);

    // direct lighting
    {
        brx_float3 L = brx_normalize(-g_LightDirection.xyz);
        brx_float3 N = shading_normal_world_space;
        brx_float3 V = -camera_ray_direction;

        brx_float raw_NdotL = brx_dot(N, L);
        brx_float NdotL = brx_clamp(raw_NdotL, 0.0, 1.0);
        brx_float NdotV = brx_clamp(brx_dot(N, V), 0.0, 1.0);
        brx_float VdotL = brx_dot(V, L);

        // Real-Time Rendering Fourth Edition / 9.8 BRDF Models for Surface Reflection / [Hammon 2017]
        //
        // |L + V|^2 = L^2 + V^2 + 2L·V = 1 + 1 + 2L·V
        // N·H = N·((L + V)/(|L + V|)) = (N·L + N·V)/(|L + V|)
        // L·H = L·((L + V)/(|L + V|)) = (L^2 + L·V)/(|L + V|)
        // V·H = V·((L + V)/(|L + V|)) = (L·V + V^2)/(|L + V|)
        // 2L·H = 2V·H = L·H + V·H = (L^2 + L·V + L·V + V^2)/(|L + V|) = (|L + V|^2)/(|L + V|) = |L + V| = 1 + 1 + 2L·V
        // ⇒ L·H = 0.5 * |L + V| = (0.5 * |L + V|^2)/(|L + V|) =(0.5 * (1 + 1 + 2L·V))/(|L + V|) = 1/(|L + V|) + (L·V)/(|L + V|)
        //
        // UE4: [Init](https://github.com/EpicGames/UnrealEngine/blob/4.27/Engine/Shaders/Private/BRDF.ush#L31)
        // U3D: [GetBSDFAngle](https://github.com/Unity-Technologies/Graphics/blob/v10.8.0/com.unity.render-pipelines.core/ShaderLibrary/CommonLighting.hlsl#L361)
        brx_float invLenH = brx_rsqrt(2.0 + 2.0 * VdotL);
        brx_float NdotH = brx_clamp((NdotL + NdotV) * invLenH, 0.0, 1.0);
        brx_float LdotH = brx_clamp(invLenH * VdotL + invLenH, 0.0, 1.0);
        brx_float VdotH = LdotH;

        float3 E_l;
        [branch] if (raw_NdotL > 0.0)
        {
            float shadow = GetShadow(surface_position_world_space);
            E_l = g_LightColor.rgb * shadow;
        }
        else
        {
            E_l = float3(0.0, 0.0, 0.0);
        }

        float3 E_n = E_l * NdotL;

        // direct diffuse
        radiance += diffuse_color * E_n; // div PI?

        // TODO: direct specular
        // D * F * V * E_n

        brx_float3 F;
        {
            // glTF Sample Renderer: [F_Schlick](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/e5646a2bf87b0871ba3f826fc2335fe117a11411/source/Renderer/shaders/brdf.glsl#L24)
            brx_float3 f0 = specular_color;
            const brx_float3 f90 = brx_float3(1.0, 1.0, 1.0);

            brx_float x = brx_clamp(1.0 - VdotH, 0.0, 1.0);
            brx_float x2 = x * x;
            brx_float x5 = x * x2 * x2;
            F = f0 + (f90 - f0) * x5;
        }
    }

    // indirect lighting
    {
        float4 indirect_radiance_and_ambient_occlusion = (g_EnableIndirectDiffuse || g_EnableIndirectSpecular) ? t_IndirectSpecular[IN.position.xy] : 0;

        float3 indirect_radiance = indirect_radiance_and_ambient_occlusion.xyz;

        float ambient_occlusion = indirect_radiance_and_ambient_occlusion.w;

        // voxel cone tracing
        radiance += indirect_radiance;

        // environment lighting
        radiance += diffuse_color * g_AmbientColor.rgb * ambient_occlusion;
    }

    return float4(ConvertToLDR(radiance), 1);
}