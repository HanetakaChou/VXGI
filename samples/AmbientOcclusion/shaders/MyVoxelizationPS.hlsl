#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_shader_language.bsli"
#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_brdf.bsli"

#define REGISTER(type, slot) register(type##slot)
#define NV_SHADER_EXTN_SLOT u0
#define VXGI_VOXELIZE_CB_SLOT 1
#define VXGI_ALLOCATION_MAP_UAV_SLOT 1
#define VXGI_OPACITY_UAV_SLOT 2
#define VXGI_EMITTANCE_EVEN_UAV_SLOT 3
#define VXGI_EMITTANCE_ODD_UAV_SLOT 4
#define VXGI_IRRADIANCE_MAP_SRV_SLOT 6
#define VXGI_IRRADIANCE_MAP_SAMPLER_SLOT 1
#define VXGI_INVALIDATE_BITMAP_SRV_SLOT 7

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

static const float2 g_VxgiSamplePositions[] = {
    float2(0.0625, -0.1875), float2(-0.0625, 0.1875), float2(0.3125, 0.0625), float2(-0.1875, -0.3125),
    float2(-0.3125, 0.3125), float2(-0.4375, 0.0625), float2(0.1875, 0.4375), float2(0.4375, -0.4375),
    float2(0, 0)};

bool VxgiCanWriteEmittance()
{

    return false;
}

RWTexture3D<uint> u_AllocationMap : REGISTER(u, VXGI_ALLOCATION_MAP_UAV_SLOT);

RWTexture3D<uint> u_Opacity : REGISTER(u, VXGI_OPACITY_UAV_SLOT);

void VxgiGetLayeredCoverage(float inputZ, uint coverage, float depthSign, float inputOpacity, out float4 layersForOpacity, out float4 layersForEmittance)
{
    layersForOpacity = float4scalar(0);
    layersForEmittance = float4scalar(0);

    if (inputOpacity <= 1)
    {
        float2 dZ = float2(ddx(inputZ), ddy(inputZ));
        float fracZ = frac(inputZ);

        [unroll] for (int nSample = 0; nSample < 8; nSample++)
        {
            if ((coverage & (1 << nSample)) != 0)
            {
                float relativeZ = fracZ + dot(dZ, g_VxgiSamplePositions[nSample]);
                const float delta = 1.0f / 8;

                layersForEmittance.x += delta * saturate(1 - abs(relativeZ + 0.5));
                layersForEmittance.y += delta * saturate(1 - abs(relativeZ - 0.5));
                layersForEmittance.z += delta * saturate(1 - abs(relativeZ - 1.5));
            }
        }

        layersForOpacity = layersForEmittance;
    }
    else
    {
        float extraDepth = 0.5 * saturate(inputOpacity - 1);
        float e1 = extraDepth + 1;
        float e2 = extraDepth * depthSign;

        float2 dZ = float2(ddx(inputZ), ddy(inputZ));
        float fracZ = frac(inputZ);
        if (depthSign < 0)
            fracZ += 1;

        [unroll] for (int nSample = 0; nSample < 8; nSample++)
        {
            if ((coverage & (1 << nSample)) != 0)
            {
                float relativeZ = fracZ + dot(dZ, g_VxgiSamplePositions[nSample]);
                const float delta = 1.0f / 8;

                layersForEmittance.x += delta * saturate(1 - abs(relativeZ + 0.5));
                layersForEmittance.y += delta * saturate(1 - abs(relativeZ - 0.5));
                layersForEmittance.z += delta * saturate(1 - abs(relativeZ - 1.5));
                layersForEmittance.w += delta * saturate(1 - abs(relativeZ - 2.5));

                layersForOpacity.x += delta * saturate(e1 - abs(relativeZ + 0.5 + e2));
                layersForOpacity.y += delta * saturate(e1 - abs(relativeZ - 0.5 + e2));
                layersForOpacity.z += delta * saturate(e1 - abs(relativeZ - 1.5 + e2));
                layersForOpacity.w += delta * saturate(e1 - abs(relativeZ - 2.5 + e2));
            }
        }
    }
}

float VxgiGetNormalProjection(float3 normal, uint direction)
{
    switch (direction)
    {
    case 0:
        return normal.x;
    case 3:
        return -normal.x;
    case 1:
        return normal.y;
    case 4:
        return -normal.y;
    case 2:
        return normal.z;
    case 5:
        return -normal.z;
    }
}

void VxgiWriteVoxel(int3 coordinates, int clipLevel, bool writeOpacity, bool writeEmittance, float perVoxelScaleOpacity, float perVoxelScaleEmittance, float opacity, float3 emittanceFront, float3 emittanceBack, float3 normal, inout int prevPageCoordinatesHash, inout uint page)
{
    emittanceFront = max(emittanceFront * perVoxelScaleEmittance, 0);
    emittanceBack = max(emittanceBack * perVoxelScaleEmittance, 0);
    opacity = saturate(opacity * perVoxelScaleOpacity);

    if (opacity == 0)
        writeOpacity = false;

    bool emittanceFrontPresent = any(emittanceFront != 0);
    bool emittanceBackPresent = any(emittanceBack != 0);

    if (!emittanceFrontPresent && !emittanceBackPresent)
        writeEmittance = false;

    if (!writeOpacity && !writeEmittance)
    {
        return;
    }

    int3 tmp = coordinates.xyz & ~(int3(g_VxgiVoxelizationCB.ClipLevelSize.xyz) - int3scalar(1));
    if ((tmp.x | tmp.y | tmp.z) != 0)
    {
        return;
    }

    {
        int4 translation = g_VxgiVoxelizationCB.TextureToAmapTranslation[clipLevel];
        int3 pageCoordinates = int3(((coordinates >> translation.w) + translation.xyz) & (g_VxgiVoxelizationCB.AllocationMapSize.xyz - 1));
        int pageCoordinatesHash = pageCoordinates.x + pageCoordinates.y + pageCoordinates.z;

        if (pageCoordinatesHash != prevPageCoordinatesHash)
            page = u_AllocationMap[pageCoordinates].x;

        prevPageCoordinatesHash = pageCoordinatesHash;

        if ((page & 0x04) == 0)
            writeOpacity = false;

        if ((page & 0x08) == 0)
            writeEmittance = false;

        if (!writeOpacity && !writeEmittance)
        {
            return;
        }

        uint newPage = page;
        if (writeOpacity || writeEmittance)
            newPage |= 0x01;
        if (writeEmittance)
            newPage |= 0x02;

        if (page != newPage)
        {
            InterlockedOr(u_AllocationMap[pageCoordinates], newPage);
        }

        page = newPage;
    }

    int3 voxelCoords = int3(VxgiGetToroidalAddress(coordinates, g_VxgiVoxelizationCB.ToroidalOffset.xyz >> clipLevel, g_VxgiVoxelizationCB.ClipLevelSize.xyz));
    int3 opacityAddress = voxelCoords + int3(0, 0, int(g_VxgiVoxelizationCB.LevelStride) * clipLevel + 1);

    if (writeOpacity)
    {
        InterlockedAdd(u_Opacity[opacityAddress], uint(opacity * 1023));
    }
}

#define BRX_VCT_VOXELIZATION_ENABLE_ILLUMINATION 0

#include "../../../thirdparty/Voxel-Cone-Tracing/shaders/brx_voxel_cone_tracing_voxelization_fragment.bsli"

//////////////////APP CODE BOUNDARY/////////////

Texture2D g_normal_texture : register(t3);
Texture2D g_base_color_texture : register(t4);
Texture2D g_roughness_metallic_texture : register(t5);

SamplerState g_sampler : register(s0);

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

    // [branch]
    if (opacity < 0.5)
    {
        discard;
        return;
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
        brx_normalize(brx_float3(-1.0, -1.0, -1.0)),
        brx_float3(0.0, 0.0, 0.0));

    discard;
}

//////////////////APP CODE BOUNDARY/////////////
