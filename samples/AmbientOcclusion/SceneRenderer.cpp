/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <string>
#include "SceneRenderer.h"
#include "BindingHelpers.h"
#include "DirectXMath.h"

#if USE_GL4
#include "shaders\GL\DefaultVS.glsl"
#include "shaders\GL\VoxelizationVS.glsl"
#include "shaders\GL\AttributesPS.glsl"
#include "shaders\GL\FullScreenQuadVS.glsl"
#include "shaders\GL\BlitPS.glsl"
#include "shaders\GL\CompositingPS.glsl"
#include "shaders\GL\VoxelizationPS.glsl"
#else
#include "shaders\D3D\DefaultVS.inl"
#include "shaders\D3D\VoxelizationVS.inl"
#include "shaders\D3D\AttributesPS.inl"
#include "shaders\D3D\FullScreenQuadVS.inl"
#include "shaders\D3D\BlitPS.inl"
#include "shaders\D3D\CompositingPS.inl"
#include "shaders\VoxelizationPS.hlsli"
#endif

static const UINT s_ShadowMapSize = 2048;

using namespace DirectX;

SceneRenderer::SceneRenderer(NVRHI::IRendererInterface *pRenderer)
    : m_RendererInterface(pRenderer), m_pScene(NULL), m_pInputLayout(NULL), m_pDefaultVS(NULL), m_pVoxelizationVS(NULL), m_pFullScreenQuadVS(NULL), m_pAttributesPS(NULL), m_pBlitPS(NULL), m_pCompositingPS(NULL), m_pGlobalCBuffer(NULL), m_pDefaultSamplerState(NULL), m_Width(0), m_Height(0), m_SampleCount(1), m_pVoxelizationGS(NULL), m_pVoxelizationPS(NULL), m_TargetAlbedo(NULL), m_TargetNormal(NULL), m_TargetDepth(NULL), m_NullTexture(NULL)
{
}

HRESULT SceneRenderer::LoadMesh(const char *strFileName)
{
    m_pScene = new Scene();
    return m_pScene->Load(strFileName);
}

HRESULT SceneRenderer::AllocateResources(VXGI::IGlobalIllumination *pGI, VXGI::IShaderCompiler *pCompiler)
{
    m_pDefaultVS = CREATE_SHADER(VERTEX, g_DefaultVS);

    const NVRHI::VertexAttributeDesc SceneLayout[] =
        {
            {"POSITION", NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, position), false},
            {"NORMAL", NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, normal), false}};

#if USE_GL4
    m_pInputLayout = m_RendererInterface->createInputLayout(SceneLayout, _countof(SceneLayout), nullptr, 0);
#else
    m_pInputLayout = m_RendererInterface->createInputLayout(SceneLayout, _countof(SceneLayout), g_DefaultVS, sizeof(g_DefaultVS));
#endif

    m_pVoxelizationVS = CREATE_SHADER(VERTEX, g_VoxelizationVS);
    m_pFullScreenQuadVS = CREATE_SHADER(VERTEX, g_FullScreenQuadVS);
    m_pAttributesPS = CREATE_SHADER(PIXEL, g_AttributesPS);
    m_pBlitPS = CREATE_SHADER(PIXEL, g_BlitPS);
    m_pCompositingPS = CREATE_SHADER(PIXEL, g_CompositingPS);

    m_pGlobalCBuffer = m_RendererInterface->createConstantBuffer(NVRHI::ConstantBufferDesc(sizeof(GlobalConstants), nullptr), nullptr);

    NVRHI::SamplerDesc samplerDesc;
    samplerDesc.wrapMode[0] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[1] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[2] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.minFilter = samplerDesc.magFilter = samplerDesc.mipFilter = true;
    samplerDesc.anisotropy = 16;
    m_pDefaultSamplerState = m_RendererInterface->createSampler(samplerDesc);

    NVRHI::TextureDesc nullTextureDesc;
    nullTextureDesc.width = 1;
    nullTextureDesc.height = 1;
    nullTextureDesc.format = NVRHI::Format::RGBA8_UNORM;
    nullTextureDesc.debugName = "NullTexture";
    uint32_t nullData = 0;
    m_NullTexture = m_RendererInterface->createTexture(nullTextureDesc, &nullData);

    if (FAILED(m_pScene->InitResources(m_RendererInterface)))
        return E_FAIL;

    // Voxelization GS
    {
        VXGI::IBlob *blob = nullptr;

#if USE_GL4
        {
            VXGI::VoxelizationGeometryShaderDesc desc;
            desc.pixelShaderInputCount = 0;

            if (VXGI_FAILED(pCompiler->compileVoxelizationGeometryShader(&blob, desc)))
                return E_FAIL;
        }
#else
        if (VXGI_FAILED(pCompiler->compileVoxelizationGeometryShaderFromVS(&blob, g_VoxelizationVS, sizeof(g_VoxelizationVS))))
            return E_FAIL;
#endif

        if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pVoxelizationGS, blob->getData(), blob->getSize())))
            return E_FAIL;

        blob->dispose();
    }

    // Voxelization PS
    {
        VXGI::IBlob *blob = nullptr;

        // Enumerate resource slots (constant buffers, textures, samplers, potentially UAVs)
        // that are used by the user part of the voxelization pixel shader
        VXGI::ShaderResources resources;
        resources.constantBufferCount = 0;
        resources.textureCount = 0;
        resources.samplerCount = 0;

        VXGI::VoxelizationPixelShaderDesc desc;
        desc.userShaderCodeResources = &resources;
        desc.source = g_VoxelizationPS;
        desc.sourceSize = sizeof(g_VoxelizationPS);

        if (VXGI_FAILED(pCompiler->compileVoxelizationPixelShader(&blob, desc)))
            return E_FAIL;

        if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pVoxelizationPS, blob->getData(), blob->getSize())))
            return E_FAIL;

        blob->dispose();
    }

    return S_OK;
}

void SceneRenderer::AllocateViewDependentResources(UINT width, UINT height, UINT sampleCount)
{
    m_Width = width;
    m_Height = height;
    m_SampleCount = sampleCount;

    NVRHI::TextureDesc gbufferDesc;
    gbufferDesc.width = m_Width;
    gbufferDesc.height = m_Height;
    gbufferDesc.isRenderTarget = true;
    gbufferDesc.useClearValue = true;
    gbufferDesc.sampleCount = m_SampleCount;
    gbufferDesc.disableGPUsSync = true;

    gbufferDesc.format = NVRHI::Format::RGBA8_UNORM;
    gbufferDesc.clearValue = NVRHI::Color(0.f);
    gbufferDesc.debugName = "GbufferC (BaseColor + Metallic)";
    m_TargetAlbedo = m_RendererInterface->createTexture(gbufferDesc, NULL);

    gbufferDesc.format = NVRHI::Format::RGBA16_FLOAT;
    gbufferDesc.clearValue = NVRHI::Color(0.f);
    gbufferDesc.debugName = "GbufferA (Normal + Roughness)";
    m_TargetNormal = m_RendererInterface->createTexture(gbufferDesc, NULL);

    gbufferDesc.format = NVRHI::Format::D32;
    gbufferDesc.clearValue = NVRHI::Color(1.f, 0.f, 0.f, 0.f);
    gbufferDesc.debugName = "Depth";
    m_TargetDepth = m_RendererInterface->createTexture(gbufferDesc, NULL);
}

void SceneRenderer::ReleaseResources(VXGI::IGlobalIllumination *pGI)
{
    if (m_pScene)
    {
        delete m_pScene;
        m_pScene = NULL;
    }

    DESTROY_INPUT_LAYOUT(m_pInputLayout);
    DESTROY_SHADER(m_pDefaultVS);
    DESTROY_SHADER(m_pVoxelizationVS);
    DESTROY_SHADER(m_pFullScreenQuadVS);
    DESTROY_SHADER(m_pAttributesPS);
    DESTROY_SHADER(m_pBlitPS);
    DESTROY_SHADER(m_pCompositingPS);

    DESTROY_CONSTANT_BUFFER(m_pGlobalCBuffer);

    DESTROY_SAMPLER(m_pDefaultSamplerState);

    DESTROY_TEXTURE(m_NullTexture);

    if (m_pVoxelizationGS)
    {
        pGI->destroyUserDefinedShaderSet(m_pVoxelizationGS);
        m_pVoxelizationGS = NULL;
    }

    if (m_pVoxelizationPS)
    {
        pGI->destroyUserDefinedShaderSet(m_pVoxelizationPS);
        m_pVoxelizationPS = NULL;
    }
}

void SceneRenderer::ReleaseViewDependentResources()
{
    DESTROY_TEXTURE(m_TargetAlbedo);
    DESTROY_TEXTURE(m_TargetNormal);
    DESTROY_TEXTURE(m_TargetDepth);
}

void SceneRenderer::RenderToGBuffer(const VXGI::Matrix4f &viewProjMatrix)
{
    NVRHI::DrawCallState state;

    MaterialCallback onChangeMaterial = [this, &state](const MeshMaterialInfo &material)
    {
        NVRHI::BindConstantBuffer(state.PS, 1, material.materialBuffer);
    };

    state.PS.shader = m_pAttributesPS;
    NVRHI::BindSampler(state.PS, 0, m_pDefaultSamplerState);

    state.renderState.targetCount = 2;
    state.renderState.targets[0] = m_TargetAlbedo;
    state.renderState.targets[1] = m_TargetNormal;
    state.renderState.clearColorTarget = true;
    state.renderState.depthTarget = m_TargetDepth;
    state.renderState.clearDepthTarget = true;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(m_Width), float(m_Height));

    RenderSceneCommon(state, NULL, NULL, 0, viewProjMatrix, &onChangeMaterial, false);
}

void SceneRenderer::RenderSceneCommon(
    NVRHI::DrawCallState &state,
    VXGI::IGlobalIllumination *pGI,
    const VXGI::Box3f *clippingBoxes,
    uint32_t numBoxes,
    const VXGI::Matrix4f &viewProjMatrix,
    MaterialCallback *onChangeMaterial,
    bool voxelization)
{
    GlobalConstants globalConstants;
    globalConstants.viewProjMatrix = viewProjMatrix;
    m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &globalConstants, sizeof(globalConstants));

    state.inputLayout = m_pInputLayout;
    state.primType = NVRHI::PrimitiveType::TRIANGLE_LIST;
    state.indexBufferFormat = NVRHI::Format::R32_UINT;
    state.indexBuffer = m_pScene->GetIndexBuffer();

    state.vertexBufferCount = 1;
    state.vertexBuffers[0].buffer = m_pScene->GetVertexBuffer();
    state.vertexBuffers[0].slot = 0;
    state.vertexBuffers[0].stride = sizeof(VertexBufferEntry);

    bool extraStateSetup = false;

    NVRHI::BindConstantBuffer(state.VS, 0, m_pGlobalCBuffer);

    state.renderState.rasterState.frontCounterClockwise = true;

    if (voxelization)
    {
        state.VS.shader = m_pVoxelizationVS;
        pGI->beginVoxelizationDrawCallGroup();
    }
    else
        state.VS.shader = m_pDefaultVS;

    int lastMaterial = -2;
    VXGI::MaterialInfo lastMaterialInfo;

    UINT numMeshes = m_pScene->GetMeshesNum();

    std::vector<NVRHI::DrawArguments> drawCalls;

    for (UINT i = 0; i < numMeshes; ++i)
    {
        VXGI::Box3f meshBounds = m_pScene->GetMeshBounds(i);

        if (clippingBoxes && numBoxes)
        {
            bool contained = false;
            for (UINT clipbox = 0; clipbox < numBoxes; ++clipbox)
            {
                if (clippingBoxes[clipbox].intersectsWith(meshBounds))
                {
                    contained = true;
                    break;
                }
            }

            if (!contained)
                continue;
        }

        int material = m_pScene->GetMaterialIndex(i);

        if (material != lastMaterial)
        {
            if (!drawCalls.empty())
            {
                m_RendererInterface->drawIndexed(state, &drawCalls[0], uint32_t(drawCalls.size()));
                drawCalls.clear();

                state.renderState.clearDepthTarget = false;
                state.renderState.clearColorTarget = false;
            }

            MeshMaterialInfo materialInfo;
            GetMaterialInfo(i, materialInfo);

            if (voxelization)
            {
                if (lastMaterial < 0 || lastMaterialInfo != materialInfo)
                {
                    NVRHI::DrawCallState voxelizationState;
                    if (VXGI_FAILED(pGI->getVoxelizationState(materialInfo, voxelizationState)))
                        continue;

                    if (voxelizationState.renderState.setupExtraVoxelizationState && !extraStateSetup)
                    {
                        pGI->setupExtraVoxelizationState();
                        extraStateSetup = true;
                    }

                    state.GS = voxelizationState.GS;
                    state.PS = voxelizationState.PS;
                    state.renderState = voxelizationState.renderState;
                    state.renderState.rasterState.frontCounterClockwise = false;
                }
            }

            if (onChangeMaterial)
            {
                (*onChangeMaterial)(materialInfo);
            }

            lastMaterial = material;
            lastMaterialInfo = materialInfo;
        }

        drawCalls.push_back(m_pScene->GetMeshDrawArguments(i));
    }

    if (!drawCalls.empty())
    {
        m_RendererInterface->drawIndexed(state, &drawCalls[0], uint32_t(drawCalls.size()));
    }

    if (voxelization)
    {
        if (extraStateSetup)
            pGI->removeExtraVoxelizationState();

        pGI->endVoxelizationDrawCallGroup();
    }
}

void SceneRenderer::GetMaterialInfo(UINT meshID, OUT MeshMaterialInfo &materialInfo)
{
    materialInfo.materialBuffer = m_pScene->GetMaterialBuffer(meshID);

    materialInfo.voxelizationThickness = 1.0f;
    materialInfo.twoSided = false;

    materialInfo.geometryShader = m_pVoxelizationGS;
    materialInfo.pixelShader = m_pVoxelizationPS;
}

void SceneRenderer::FillTracingInputBuffers(VXGI::IViewTracer::InputBuffers &inputBuffers)
{
    inputBuffers.gbufferViewport = NVRHI::Viewport(float(m_Width), float(m_Height));
    inputBuffers.gbufferDepth = m_TargetDepth;
    inputBuffers.gbufferNormal = m_TargetNormal;
}

NVRHI::TextureHandle SceneRenderer::GetAlbedoBufferHandle()
{
    return m_TargetAlbedo;
}

void SceneRenderer::Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest)
{
    NVRHI::DrawCallState state;

    state.primType = NVRHI::PrimitiveType::TRIANGLE_STRIP;
    state.VS.shader = m_pFullScreenQuadVS;
    state.PS.shader = m_pBlitPS;

    state.renderState.targetCount = 1;
    state.renderState.targets[0] = pDest;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(m_Width), float(m_Height));
    state.renderState.depthStencilState.depthEnable = false;
    state.renderState.rasterState.cullMode = NVRHI::RasterState::CULL_NONE;

    NVRHI::BindTexture(state.PS, 0, pSource);

    NVRHI::DrawArguments args;
    args.vertexCount = 4;
    m_RendererInterface->draw(state, &args, 1);
}

void SceneRenderer::ComposeAO(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest)
{
    NVRHI::DrawCallState state;

    state.primType = NVRHI::PrimitiveType::TRIANGLE_STRIP;
    state.VS.shader = m_pFullScreenQuadVS;
    state.PS.shader = m_pCompositingPS;

    state.renderState.targetCount = 1;
    state.renderState.targets[0] = pDest;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(m_Width), float(m_Height));
    state.renderState.depthStencilState.depthEnable = false;
    state.renderState.rasterState.cullMode = NVRHI::RasterState::CULL_NONE;

    NVRHI::BindTexture(state.PS, 0, pSource);
    NVRHI::BindTexture(state.PS, 1, m_TargetAlbedo);

    NVRHI::DrawArguments args;
    args.vertexCount = 4;
    m_RendererInterface->draw(state, &args, 1);
}
