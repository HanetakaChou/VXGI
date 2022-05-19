#ifndef _GLOBAL_CONSTANTS_H_
#define _GLOBAL_CONSTANTS_H_ 1

#include "../../thirdparty/Voxel-Cone-Tracing/include/brx_voxel_cone_tracing.h"

#if defined(__STDC__) || defined(__cplusplus)

__declspec(align(16)) struct GlobalConstants
{
    VXGI::float4x4 worldMatrix;
    VXGI::float4x4 viewProjMatrix;
    VXGI::float4x4 viewProjMatrixInv;
    VXGI::float4x4 lightMatrix;
    DirectX::XMFLOAT4X4 viewport_depth_direction_view_matrices[BRX_VCT_VIEWPORT_DEPTH_DIRECTION_COUNT];
    DirectX::XMFLOAT4X4 clipmap_stack_level_projection_matrices[BRX_VCT_CLIPMAP_STACK_LEVEL_COUNT];
    DirectX::XMFLOAT4 clipmap_center;
    VXGI::float4 cameraPos;
    VXGI::float4 lightDirection;
    VXGI::float4 diffuseColor;
    VXGI::float4 lightColor;
    VXGI::float4 ambientColor;
    float rShadowMapSize;
    uint32_t enableIndirectDiffuse;
    uint32_t enableIndirectSpecular;
    float transparentRoughness;
    float transparentReflectance;
};
#elif defined(HLSL_VERSION) || defined(__HLSL_VERSION)

cbuffer GlobalConstants : register(b0)
{
    float4x4 g_WorldMatrix;
    float4x4 g_ViewProjMatrix;
    float4x4 g_ViewProjMatrixInv;
    float4x4 g_LightViewProjMatrix;
    float4x4 g_viewport_depth_direction_view_matrices[BRX_VCT_VIEWPORT_DEPTH_DIRECTION_COUNT];
    float4x4 g_clipmap_stack_level_projection_matrices[BRX_VCT_CLIPMAP_STACK_LEVEL_COUNT];
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

#else
#error Unknown Compiler
#endif

#endif