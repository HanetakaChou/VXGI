#ifndef _GLOBAL_CONSTANTS_H_
#define _GLOBAL_CONSTANTS_H_ 1

#if defined(__STDC__) || defined(__cplusplus)

__declspec(align(16)) struct GlobalConstants
{
    VXGI::float4x4 worldMatrix;
    VXGI::float4x4 viewProjMatrix;
    DirectX::XMFLOAT4X4 viewport_depth_direction_view_matrices[3];
    DirectX::XMFLOAT4X4 clipmap_stack_level_projection_matrices[5];
    DirectX::XMFLOAT4 clipmap_center;
    uint32_t visualizeAO;
    uint32_t _unused_padding_1;
    uint32_t _unused_padding_2;
    uint32_t _unused_padding_3;
};

#elif defined(HLSL_VERSION) || defined(__HLSL_VERSION)

cbuffer GlobalConstants : register(b0)
{
    float4x4 g_WorldMatrix;
    float4x4 g_ViewProjMatrix;
    float4x4 g_viewport_depth_direction_view_matrices[3];
    float4x4 g_clipmap_stack_level_projection_matrices[5];
    float4 g_clipmap_center;
    uint g_VisualizeAO;
    uint _unused_padding_1;
    uint _unused_padding_2;
    uint _unused_padding_3;
}

#else
#error Unknown Compiler
#endif

#endif