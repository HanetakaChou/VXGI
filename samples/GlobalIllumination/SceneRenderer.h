/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

#include "GFSDK_VXGI.h"
#include "Scene.h"
#include <functional>

#pragma warning(disable : 4324)

__declspec(align(16)) struct GlobalConstants
{
    VXGI::Matrix4f viewProjMatrix;
    VXGI::Matrix4f viewProjMatrixInv;
    VXGI::Matrix4f lightMatrix;
    VXGI::Vector4f lightPos;
    VXGI::Vector4f lightColor;
    VXGI::Vector4f ambientColor;
    float rShadowMapSize;
    uint32_t enableIndirectDiffuse;
    uint32_t enableIndirectSpecular;
};

struct MeshMaterialInfo : public VXGI::MaterialInfo
{
    NVRHI::ConstantBufferHandle materialBuffer;

    MeshMaterialInfo() : materialBuffer(NULL)
    {
    }
};

typedef std::function<void(const MeshMaterialInfo &)> MaterialCallback;

class SceneRenderer
{
private:
    Scene *m_pScene;
    NVRHI::IRendererInterface *m_RendererInterface;

    NVRHI::InputLayoutHandle m_pInputLayout;
    NVRHI::ShaderHandle m_pDefaultVS;
    NVRHI::ShaderHandle m_pFullScreenQuadVS;

    NVRHI::ShaderHandle m_pAttributesPS;
    NVRHI::ShaderHandle m_pBlitPS;
    NVRHI::ShaderHandle m_pCompositingPS;

    NVRHI::ConstantBufferHandle m_pGlobalCBuffer;

    NVRHI::SamplerHandle m_pDefaultSamplerState;
    NVRHI::SamplerHandle m_pComparisonSamplerState;

    UINT m_Width;
    UINT m_Height;
    UINT m_SampleCount;

    VXGI::Vector3f m_LightPos;
    VXGI::Matrix4f m_LightViewMatrix;
    VXGI::Matrix4f m_LightProjMatrix;
    VXGI::Matrix4f m_LightViewProjMatrix;

    NVRHI::TextureHandle m_TargetAlbedo;
    NVRHI::TextureHandle m_TargetNormal;
    NVRHI::TextureHandle m_TargetDepth;

    NVRHI::TextureHandle m_ShadowMap;

    NVRHI::TextureHandle m_NullTexture;

    VXGI::IUserDefinedShaderSet *m_pVoxelizationGS;
    VXGI::IUserDefinedShaderSet *m_pVoxelizationPS;

public:
    SceneRenderer(NVRHI::IRendererInterface *pRenderer);

    HRESULT LoadMesh(const char *strFileName);

    HRESULT AllocateResources(VXGI::IGlobalIllumination *pGI, VXGI::IShaderCompiler *pCompiler);
    void AllocateViewDependentResources(UINT width, UINT height, UINT sampleCount = 1);
    void ReleaseResources(VXGI::IGlobalIllumination *pGI);
    void ReleaseViewDependentResources();

    void RenderToGBuffer(const VXGI::Matrix4f &viewProjMatrix);

    void RenderShadowMap(const VXGI::Vector3f &lightPos, const VXGI::Matrix4f &lightViewMatrix, const VXGI::Matrix4f &lightProjMatrix);

    void GetMaterialInfo(UINT meshID, OUT MeshMaterialInfo &materialInfo);

    void FillTracingInputBuffers(VXGI::IViewTracer::InputBuffers &inputBuffers);
    NVRHI::TextureHandle GetAlbedoBufferHandle();

    void Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest);
    void Shade(
        NVRHI::TextureHandle indirectDiffuse,
        NVRHI::TextureHandle indirectSpecular,
        NVRHI::TextureHandle pDest,
        const VXGI::Matrix4f &viewProjMatrix,
        VXGI::Vector3f ambientColor);

    void RenderSceneCommon(
        NVRHI::DrawCallState &state,
        VXGI::IGlobalIllumination *pGI,
        const VXGI::Box3f *clippingBoxes,
        uint32_t numBoxes,
        const VXGI::Matrix4f &viewProjMatrix,
        MaterialCallback *onChangeMaterial,
        bool voxelization,
        bool emittance);

    VXGI::Frustum GetLightFrustum();
};