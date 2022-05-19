#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_shader_language.bsli"
#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_brdf.bsli"
#define BRX_VCT_CLIPMAP_OPACITY_TEXTURE u_Opacity
#define BRX_VCT_CLIPMAP_ILLUMINATION_TEXTURE u_Emittance
#include "../../../thirdparty/Voxel-Cone-Tracing/include/brx_voxel_cone_tracing.h"

#define REGISTER(type, slot) register(type##slot)
#define NV_SHADER_EXTN_SLOT u0
#define VXGI_VOXELIZE_CB_SLOT 1
#define VXGI_ALLOCATION_MAP_UAV_SLOT 1
#define VXGI_OPACITY_UAV_SLOT 2
#define VXGI_EMITTANCE_EVEN_UAV_SLOT 3
#define VXGI_EMITTANCE_ODD_UAV_SLOT 4
#define VXGI_IRRADIANCE_MAP_SRV_SLOT 7
#define VXGI_IRRADIANCE_MAP_SAMPLER_SLOT 2
#define VXGI_INVALIDATE_BITMAP_SRV_SLOT 8

#pragma pack_matrix(row_major)

static const float VxgiPI = 3.14159265;

float2 float2scalar(float x) { return float2(x, x); }
float3 float3scalar(float x) { return float3(x, x, x); }
float4 float4scalar(float x) { return float4(x, x, x, x); }
int2 int2scalar(int x) { return int2(x, x); }
int3 int3scalar(int x) { return int3(x, x, x); }
int4 int4scalar(int x) { return int4(x, x, x, x); }
uint2 uint2scalar(uint x) { return uint2(x, x); }
uint3 uint3scalar(uint x) { return uint3(x, x, x); }
uint4 uint4scalar(uint x) { return uint4(x, x, x, x); }

struct VxgiFullScreenQuadOutput
{
    float2 uv : TEXCOORD;
    float4 posProj : RAY;
    float instanceID : INSTANCEID;
};

float3 VxgiProjToRay(float4 posProj, float3 cameraPos, float4x4 viewProjMatrixInv)
{
    float4 farPoint = mul(posProj, viewProjMatrixInv);
    farPoint.xyz /= farPoint.w;

    return normalize(farPoint.xyz - cameraPos.xyz);
}

struct VxgiBox4i
{
    int4 lower;
    int4 upper;
};

struct VxgiBox4f
{
    float4 lower;
    float4 upper;
};

bool VxgiPartOfRegion(int3 coords, VxgiBox4i region)
{
    return coords.x <= region.upper.x && coords.y <= region.upper.y && coords.z <= region.upper.z &&
           coords.x >= region.lower.x && coords.y >= region.lower.y && coords.z >= region.lower.z;
}

bool VxgiPartOfRegionWithBorder(int3 coords, VxgiBox4i region, int border)
{
    return coords.x <= region.upper.x + border && coords.y <= region.upper.y + border && coords.z <= region.upper.z + border &&
           coords.x >= region.lower.x - border && coords.y >= region.lower.y - border && coords.z >= region.lower.z - border;
}

bool VxgiPartOfRegionf(float3 coords, float3 lower, float3 upper)
{
    return coords.x <= upper.x && coords.y <= upper.y && coords.z <= upper.z &&
           coords.x >= lower.x && coords.y >= lower.y && coords.z >= lower.z;
}

uint3 VxgiPackEmittanceForAtomic(float3 v)
{
    float fixedPointScale = 1 << 20;
    return uint3(v.rgb * fixedPointScale);
}

uint VxgiPackOpacity(float opacity)
{
    return uint(1023 * opacity);
}

float VxgiUnpackOpacity(uint opacityBits)
{
    return float((opacityBits) & 0x3ff) / 1023.0;
}

float VxgiAverage4(float a, float b, float c, float d)
{
    return (a + b + c + d) / 4;
}

bool VxgiIsOdd(int x)
{
    return (x & 1) != 0;
}

float VxgiMax3(float3 v)
{
    return max(v.x, max(v.y, v.z));
}

float VxgiNormalizedDistance(float2 p, float a, float b)
{
    return sqrt(p.x / a * p.x / a + p.y / b + p.y / b);
}

float3 VxgiRayPlaneIntersection(float3 rayStart, float3 rayDir, float3 planeNormal, float3 planePoint)
{

    float t = dot(planePoint - rayStart, planeNormal) / dot(rayDir, planeNormal);
    return rayStart + t * rayDir;
}

float VxgiMin3(float3 v)
{
    return min(v.x, min(v.y, v.z));
}

int3 VxgiGetToroidalAddress(int3 localPos, int3 offset, uint3 textureSize)
{
    return (localPos + offset) & (int3(textureSize) - int3scalar(1));
}

struct VxgiVoxelizationConstants
{
    float4 GridCenterPrevious;
    float4 rGridWorldSizePrevious;
    int4 ToroidalOffset;
    VxgiBox4f ScissorRegionsClipSpaceOpacity[5];
    VxgiBox4f ScissorRegionsClipSpaceEmittance[5];
    int4 TextureToAmapTranslation[5];
    float4 ResolutionFactors[5];
    uint4 AllocationMapSize;
    float4 IrradianceMapSize;
    uint4 ClipLevelSize;
    float4 CullingScale;
    uint DirectionStride;
    uint ChannelStride;
    uint LevelStride;
    uint ClipLevelMask;
    uint MaxClipLevel;
    float EmittanceStorageScale;
    uint UseIrradianceMap;
    uint PersistentVoxelData;
    uint UseInvalidateBitmap;
    uint CanWriteEmittance;
};

cbuffer VoxelizationCB : REGISTER(b, VXGI_VOXELIZE_CB_SLOT)
{
    VxgiVoxelizationConstants g_VxgiVoxelizationCB;
};

bool VxgiCanWriteEmittance()
{
    return g_VxgiVoxelizationCB.CanWriteEmittance != 0;
}

RWTexture3D<uint> u_AllocationMap : REGISTER(u, VXGI_ALLOCATION_MAP_UAV_SLOT);

RWTexture3D<uint> u_Opacity : REGISTER(u, VXGI_OPACITY_UAV_SLOT);

RWTexture3D<uint> u_Emittance : REGISTER(u, VXGI_EMITTANCE_EVEN_UAV_SLOT);

#define BRX_VCT_VOXELIZATION_ENABLE_ILLUMINATION 1

#include "../../../thirdparty/Voxel-Cone-Tracing/shaders/brx_voxel_cone_tracing_voxelization_fragment.bsli"

//////////////////APP CODE BOUNDARY/////////////

#include "../GlobalConstants.h"

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
    in uint in_sample_mask : SV_Coverage,
    in float4 in_position : SV_Position,
    in float in_cull_distance[2] : SV_CullDistance,
    in nointerpolation int in_viewport_depth_direction_index : LOCATION0,
    in nointerpolation int in_clipmap_stack_level_index : LOCATION1,
    in float3 in_interpolated_position_world_space : LOCATION2,
    in float3 in_interpolated_normal : LOCATION3,
    in float4 in_interpolated_tangent : LOCATION4,
    in float2 in_interpolated_texcoord : LOCATION5)
{
    float opacity;
    float3 shading_normal_world_space;
    float3 diffuse_color;
    float3 specular_color;
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

    // [branch]
    if (opacity < 0.5)
    {
        discard;
        return;
    }

    float3 surface_position_world_space = in_interpolated_position_world_space;

    float3 incident_direction = normalize(-g_LightDirection.xyz);

    float3 E_l;
    {
        float3 L = incident_direction;
        float3 N = shading_normal_world_space;

        float NdotL = dot(N, L);

        [branch] if (NdotL > 0.0)
        {
            float shadow = GetShadow(surface_position_world_space);

            E_l = g_LightColor.rgb * shadow;
        }
        else
        {
            E_l = float3(0.0, 0.0, 0.0);
        }
    }

    brx_voxel_cone_tracing_voxelization_store_data(
        in_viewport_depth_direction_index,
        in_clipmap_stack_level_index,
        brx_float3(in_position.x, BRX_VCT_CLIPMAP_MAP_SIZE - in_position.y, in_position.z * BRX_VCT_CLIPMAP_MAP_SIZE),
        in_sample_mask,
        opacity,
        shading_normal_world_space,
        diffuse_color,
        specular_color,
        roughness,
        incident_direction,
        E_l);

    discard;
}

//////////////////APP CODE BOUNDARY/////////////
