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
};

struct MeshMaterialInfo : public VXGI::MaterialInfo
{
    NVRHI::TextureHandle diffuseTexture;
    NVRHI::TextureHandle specularTexture;
    NVRHI::TextureHandle normalsTexture;
    VXGI::Vector3f diffuseColor;

    MeshMaterialInfo() : diffuseTexture(NULL),
                         specularTexture(NULL),
                         normalsTexture(NULL),
                         diffuseColor(0.f)
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
    NVRHI::ShaderHandle m_pVoxelizationVS;
    NVRHI::ShaderHandle m_pFullScreenQuadVS;
    NVRHI::ShaderHandle m_pAttributesPS;
    NVRHI::ShaderHandle m_pBlitPS;
    NVRHI::ShaderHandle m_pCompositingPS;

    NVRHI::ConstantBufferHandle m_pGlobalCBuffer;

    NVRHI::SamplerHandle m_pDefaultSamplerState;

    UINT m_Width;
    UINT m_Height;
    UINT m_SampleCount;

    NVRHI::TextureHandle m_TargetAlbedo;
    NVRHI::TextureHandle m_TargetNormal;
    NVRHI::TextureHandle m_TargetDepth;

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

    void GetMaterialInfo(UINT meshID, OUT MeshMaterialInfo &materialInfo);

    void FillTracingInputBuffers(VXGI::IViewTracer::InputBuffers &inputBuffers);
    NVRHI::TextureHandle GetAlbedoBufferHandle();

    void Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest);
    void ComposeAO(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest);

    void RenderSceneCommon(
        NVRHI::DrawCallState &state,
        VXGI::IGlobalIllumination *pGI,
        const VXGI::Box3f *clippingBoxes,
        uint32_t numBoxes,
        const VXGI::Matrix4f &viewProjMatrix,
        MaterialCallback *onChangeMaterial,
        bool voxelization);
};