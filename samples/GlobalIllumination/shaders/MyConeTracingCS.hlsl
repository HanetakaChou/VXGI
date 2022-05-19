#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_shader_language.bsli"
#include "../../../thirdparty/Brioche-Shader-Language/shaders/brx_brdf.bsli"
#define BRX_VCT_CLIPMAP_OPACITY_TEXTURE t_Opacity
#define BRX_VCT_CLIPMAP_OPACITY_SAMPLER s_VoxelTextureSampler
#define BRX_VCT_CLIPMAP_ILLUMINATION_TEXTURE t_Emittance
#define BRX_VCT_CLIPMAP_ILLUMINATION_SAMPLER s_VoxelTextureSampler
#include "../../../thirdparty/Voxel-Cone-Tracing/include/brx_voxel_cone_tracing.h"

#define REGISTER(type, slot) register(type##slot)
#define VXGI_VOXELTEX_SAMPLER_SLOT 2
#define VXGI_CONE_TRACING_CB_SLOT 3
#define VXGI_CONE_TRACING_TRANSLATION_CB_SLOT 4
#define VXGI_OPACITY_SRV_SLOT 14
#define VXGI_EMITTANCE_EVEN_SRV_SLOT 15
#define VXGI_EMITTANCE_ODD_SRV_SLOT 16
#define VXGI_VIEW_TRACING_CB_SLOT 1
#define VXGI_VIEW_TRACING_SAMPLER_SLOT 0
#define VXGI_VIEW_TRACING_SAMPLER_2_SLOT 1
#define VXGI_VIEW_TRACING_TEXTURE_1_SLOT 4
#define VXGI_VIEW_TRACING_TEXTURE_2_SLOT 5
#define VXGI_VIEW_TRACING_TEXTURE_3_SLOT 6
#define VXGI_VIEW_TRACING_TEXTURE_4_SLOT 7
#define VXGI_VIEW_TRACING_TEXTURE_5_SLOT 8
#define VXGI_VIEW_TRACING_TEXTURE_6_SLOT 9
#define VXGI_VIEW_TRACING_TEXTURE_7_SLOT 10
#define VXGI_VIEW_TRACING_TEXTURE_8_SLOT 11
#define VXGI_VIEW_TRACING_TEXTURE_9_SLOT 12
#define VXGI_VIEW_TRACING_TEXTURE_10_SLOT 13
#define VXGI_AREA_LIGHT_CB_SLOT 2

#pragma pack_matrix(row_major)

Texture3D t_Opacity : REGISTER(t, VXGI_OPACITY_SRV_SLOT);

Texture3D t_Emittance : REGISTER(t, VXGI_EMITTANCE_EVEN_SRV_SLOT);

SamplerState s_VoxelTextureSampler : REGISTER(s, VXGI_VOXELTEX_SAMPLER_SLOT);

struct VxgiAbstractTracingConstants
{
    float4 rOpacityTextureSize;
    float4 rEmittanceTextureSize;
    float4 ClipmapAnchor;
    float4 NearestLevelBoundary;
    float4 SceneBoundaryLower;
    float4 SceneBoundaryUpper;
    float4 ClipmapCenter;
    float4 rClipmapSizeWorld;
    float4 TracingToroidalOffset;
    float4 StackTextureSize;
    float4 rNearestLevel0Boundary;
    float EmittancePackingStride;
    float EmittanceChannelStride;
    float FinestVoxelSize;
    float FinestVoxelSizeInv;
    float MaxMipmapLevel;
    float rEmittanceStorageScale;
};

cbuffer AbstractTracingCB : REGISTER(b, VXGI_CONE_TRACING_CB_SLOT)
{
    VxgiAbstractTracingConstants g_VxgiAbstractTracingCB;
};

cbuffer TranslationCB : REGISTER(b, VXGI_CONE_TRACING_TRANSLATION_CB_SLOT)
{
    float4 g_VxgiTranslationParameters1[13];
    float4 g_VxgiTranslationParameters2[13];
    float4 g_VxgiTranslationParameters3[13];
    float4 g_VxgiTranslationParameters4[13];
};

//////////////////APP CODE BOUNDARY/////////////

struct GBufferParameters
{
    float4x4 viewMatrixInv;
    float4x4 projMatrixInv;
    float2 viewportOrigin;
    float2 viewportSizeInv;
};

cbuffer cBuiltinGBufferParameters : register(b0)
{
    GBufferParameters g_GBuffer;
    GBufferParameters g_PreviousGBuffer;
}

Texture2D g_gbuffer_base_color_and_metallic : register(t0);
Texture2D g_gbuffer_normal_and_roughness : register(t1);
Texture2D g_gbuffer_depth : register(t2);

//////////////////APP CODE BOUNDARY/////////////

struct VxgiBuiltinTracingConstants
{
    float4x4 ViewProjMatrix;
    float4x4 ViewProjMatrixInv;
    float4 PreViewTranslation;
    float4 DownsampleScale;
    float4 RefinementGridResolution;
    float4 InternalParameters;
    uint4 NormalizationModes;
    float4 NeighborhoodClampingWidths;
    float4 TemporalReprojectionWeights;
    float4 ReprojectionCombinedWeightScales;
    int2 PixelToSave;
    int2 RandomOffset;
    float2 GridOrigin;
    float2 GbufferSizeInv;
    uint ViewIndex;
    float ConeFactor;
    int MaxSamples;
    int NumCones;
    float rNumCones;
    float EmittanceScale;
    float ReprojectionDepthWeightScale;
    float ReprojectionNormalWeightExponent;
    float InterpolationWeightThreshold;
    uint EnableRefinement;
    float AmbientDistanceDarkening;
    float AmbientNormalizationFactor;
    int ViewReprojectionSource;
    float ViewReprojectionWeightThreshold;
    float PerPixelOffsetScale;
    uint OpacityLookback;
    float DiffuseSoftness;
    float DepthDeltaSign;
    uint EnableConeJitter;
};

cbuffer cBuiltinTracingParameters : REGISTER(b, VXGI_VIEW_TRACING_CB_SLOT)
{
    VxgiBuiltinTracingConstants g_VxgiBuiltinTracingCB;
};

RWTexture2D<float4> g_radiance_and_ambient : REGISTER(u, 0);

#include "../../../thirdparty/Voxel-Cone-Tracing/shaders/brx_voxel_cone_tracing_cone_tracing_compute.bsli"
