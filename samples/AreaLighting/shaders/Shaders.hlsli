#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_shader_language.bsli"
#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_packed_vector.bsli"
#include "../../../thirdparty/Environment-Lighting/shaders/brx_octahedral_mapping.bsli"

#pragma pack_matrix( row_major )

#define SURFACE_VERTEX_POSITION_BUFFER_STRIDE 12u
#define SURFACE_VERTEX_VARYING_BUFFER_STRIDE 12u
#define SURFACE_UINT32_INDEX_BUFFER_STRIDE 4u

cbuffer GlobalConstants : register(b0)
{
    float4x4 g_WorldMatrix;
    float4x4 g_ViewProjMatrix;

    float4 g_AreaLightCenter;
    float4 g_AreaLightMajorAxis;
    float4 g_AreaLightMinorAxis;
    float4 g_AreaLightColor;
}

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

Texture2D t_GBufferAlbedo : register(t0);
Texture2D t_AreaLightDiffuse : register(t1);
Texture2D t_AreaLightSpecular : register(t2);

float4 CompositingPS(FullScreenQuadOutput IN): SV_Target
{
    float4 albedo = t_GBufferAlbedo[IN.position.xy];
    float3 diffuse = t_AreaLightDiffuse[IN.position.xy].rgb;
    float3 specular = t_AreaLightSpecular[IN.position.xy].rgb;
    float3 color = albedo.rgb * diffuse.rgb + albedo.aaa * specular.rgb;
    color = pow(color, 0.5);
    return float4(color, 1);
}

void AreaLightVS(uint id : SV_VertexID, out float4 o_position : SV_Position, out float2 o_uv : TEXCOORD)
{
    float2 uv;
    uv.x = float(~id & 1);
    uv.y = float((id >> 1) & 1);
    o_uv = uv;

    uv = 2 * uv - 1;

    float3 worldPos = g_AreaLightCenter.xyz + g_AreaLightMajorAxis.xyz * uv.x + g_AreaLightMinorAxis.xyz * uv.y;
    o_position = mul(float4(worldPos, 1), g_ViewProjMatrix);

}

Texture2D t_AreaLightTexture : register(t0);

float4 AreaLightPS(in float4 i_position : SV_Position, in float2 i_uv : TEXCOORD, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    if (isFrontFace)
    {
        float4 tex = t_AreaLightTexture.Sample(g_sampler, i_uv);

        return g_AreaLightColor * tex;
    }

    return g_AreaLightColor * 0.25;
}