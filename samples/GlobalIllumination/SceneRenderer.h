/*
 * Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <DirectXMath.h>
#include "GFSDK_VXGI.h"
#include "../Scene.h"
#include <functional>

#pragma warning(disable : 4324)

#include "GlobalConstants.h"

struct MeshMaterialInfo : public VXGI::MaterialInfo
{
    NVRHI::TextureHandle normal_texture;
    NVRHI::TextureHandle base_color_texture;
    NVRHI::TextureHandle roughness_metallic_texture;

    VXGI::float3 diffuseColor;

    MeshMaterialInfo() : normal_texture(NULL), base_color_texture(NULL), roughness_metallic_texture(NULL), diffuseColor(0.f)
    {
    }
};

typedef std::function<void(const MeshMaterialInfo &)> MaterialCallback;

class SceneRenderer
{
private:
    Scene *m_pScene;
    Scene *m_pTransparentScene;
    NVRHI::IRendererInterface *m_RendererInterface;

    NVRHI::ShaderRef m_pDefaultVS;
    NVRHI::ShaderRef m_pFullScreenQuadVS;
    NVRHI::ShaderRef m_pMyVoxelizationVS;

    NVRHI::ShaderRef m_pAttributesPS;
    NVRHI::ShaderRef m_pBlitPS;
    NVRHI::ShaderRef m_pBlitLdrPS;
    NVRHI::ShaderRef m_pCompositingPS;
    NVRHI::ShaderRef m_pMyVoxelizationPS;

    NVRHI::ConstantBufferRef m_pGlobalCBuffer;

    NVRHI::SamplerRef m_pDefaultSamplerState;
    NVRHI::SamplerRef m_pComparisonSamplerState;

    UINT m_Width;
    UINT m_Height;
    UINT m_SampleCount;

    VXGI::float3 m_LightDirection;
    VXGI::float4x4 m_LightViewMatrix;
    VXGI::float4x4 m_LightProjMatrix;
    VXGI::float4x4 m_LightViewProjMatrix;

    NVRHI::TextureRef m_TargetAlbedo;
    NVRHI::TextureRef m_TargetNormal;
    NVRHI::TextureRef m_TargetDepth;
    NVRHI::TextureRef m_TargetNormalPrev;
    NVRHI::TextureRef m_TargetDepthPrev;

    NVRHI::TextureRef m_ShadowMap;

    NVRHI::TextureRef m_NullTexture;

    VXGI::IUserDefinedShaderSet *m_pVoxelizationGS;
    VXGI::IUserDefinedShaderSet *m_pVoxelizationPS;
    VXGI::IUserDefinedShaderSet *m_pTransparentGeometryPS;

    void RenderSceneCommon(
        Scene *pScene,
        NVRHI::DrawCallState &state,
        VXGI::IGlobalIllumination *pGI,
        const VXGI::Box3f *clippingBoxes,
        uint32_t numBoxes,
        const GlobalConstants &constants,
        MaterialCallback *onChangeMaterial,
        bool voxelization);

public:
    SceneRenderer(NVRHI::IRendererInterface *pRenderer);

    HRESULT LoadMesh(const char *strFileName);
    HRESULT LoadTransparentMesh(const char *strFileName);

    HRESULT AllocateResources(VXGI::IGlobalIllumination *pGI, VXGI::IShaderCompiler *pCompiler);

    HRESULT CreateVoxelizationGS(VXGI::IShaderCompiler *pCompiler, VXGI::IGlobalIllumination *pGI);
    HRESULT CreateVoxelizationPS(VXGI::IShaderCompiler *pCompiler, VXGI::IGlobalIllumination *pGI);
    HRESULT CreateTransparentGeometryPS(VXGI::IShaderCompiler *pCompiler, VXGI::IGlobalIllumination *pGI);

    void AllocateViewDependentResources(UINT width, UINT height, UINT sampleCount = 1);
    void ReleaseResources(VXGI::IGlobalIllumination *pGI);
    void ReleaseViewDependentResources();

    void RenderToGBuffer(const VXGI::float4x4 &viewProjMatrix, VXGI::float3 cameraPos, bool drawTransparent);
    void RenderTransparentScene(VXGI::IGlobalIllumination *pGI, NVRHI::TextureHandle pDest, const VXGI::float4x4 &viewProjMatrix, VXGI::float3 cameraPos, float transparentRoughness, float transparentReflectance);

    void SetLightDirection(VXGI::float3 direction);
    void RenderShadowMap(const VXGI::float3 cameraPosition, float lightSize, bool drawTransparent);

    void GetMaterialInfo(Scene *pScene, UINT meshID, OUT MeshMaterialInfo &materialInfo);

    void FillTracingInputBuffers(VXGI::IBasicViewTracer::InputBuffers &inputBuffers);
    NVRHI::TextureHandle GetAlbedoBufferHandle();
    NVRHI::TextureHandle GetNormalBufferHandle();
    NVRHI::TextureHandle GetDepthBufferHandle();

    void Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest, bool convertToLdr);
    void Shade(
        NVRHI::TextureHandle indirectDiffuse,
        NVRHI::TextureHandle indirectSpecular,
        NVRHI::TextureHandle indirectConfidence,
        NVRHI::TextureHandle pDest,
        const VXGI::float4x4 &viewProjMatrix,
        VXGI::float3 ambientColor);

    void RenderForVoxelization(
        NVRHI::DrawCallState &state,
        VXGI::IGlobalIllumination *pGI,
        const VXGI::Box3f *clippingBoxes,
        uint32_t numBoxes,
        DirectX::XMFLOAT3 const &clipmap_anchor,
        const VXGI::float4x4 &viewProjMatrix,
        MaterialCallback *onChangeMaterial);

    VXGI::Frustum GetLightFrustum();
};