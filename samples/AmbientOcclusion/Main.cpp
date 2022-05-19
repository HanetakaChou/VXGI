/*
 * Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include "SceneRenderer.h"
#include "Camera.h"
#include "SDKmisc.h"
#include <AntTweakBar.h>

#if USE_D3D11

#include "DeviceManager11.h"
#include "GFSDK_NVRHI_D3D11.h"
#define API_STRING "D3D11"
NVRHI::RendererInterfaceD3D11 *g_pRendererInterface = NULL;
#define FLUSH_COMMAND_LIST

#elif USE_D3D12

#include "DeviceManager12.h"
#include "GFSDK_NVRHI_D3D12.h"
#define API_STRING "D3D12"
NVRHI::RendererInterfaceD3D12 *g_pRendererInterface = NULL;
#define FLUSH_COMMAND_LIST g_pRendererInterface->flushCommandList()

#endif

#include "BindingHelpers.h"
#include "shaders\GBufferLoader.hlsli"

#include <d3dcompiler.h>

#include "..\..\thirdparty\Voxel-Cone-Tracing\include\brx_voxel_cone_tracing.h"
#include "..\..\thirdparty\Voxel-Cone-Tracing\include\brx_voxel_cone_tracing_voxelization.h"

using namespace DirectX;

CFirstPersonCamera g_Camera;
DeviceManager *g_DeviceManager = NULL;
SceneRenderer *g_pSceneRenderer = NULL;
VXGI::IGlobalIllumination *g_pGI = NULL;
VXGI::IShaderCompiler *g_pGICompiler = NULL;
VXGI::IUserDefinedShaderSet *g_GIGBufferLoader = NULL;
VXGI::IViewTracer *g_pGITracer = NULL;

NVRHI::ConstantBufferRef g_pBuiltinGBufferParameters;

static float g_fCameraClipNear = 1.0f;
static float g_fCameraClipFar = 10000.0f;
static float g_fVoxelSize = BRX_VCT_CLIPMAP_FINEST_VOXEL_SIZE;
static bool g_bEnableVXAO = true;
static bool g_bVisualizeAO = false;
static bool g_bRenderHUD = true;
static bool g_bShowVoxels = false;
static bool g_bInitialized = false;
static float g_fVxaoRange = 512.0f;
static float g_fVxaoQuality = 0.25f;
static float g_fVxaoSamplingRate = 0.5f;
static float g_fVxaoScale = 2.0f;
static int g_nMapSize = BRX_VCT_CLIPMAP_MAP_SIZE;
static bool g_bTemporalFiltering = true;

VXGI::IBasicViewTracer::InputBuffers g_InputBuffersPrev;
bool g_InputBuffersPrevValid = false;

class RendererErrorCallback : public NVRHI::IErrorCallback
{
    void signalError(const char *file, int line, const char *errorDesc)
    {
        char buffer[4096];
        int length = (int)strlen(errorDesc);
        length = std::min(length, 4000); // avoid a "buffer too small" exception for really long error messages
        sprintf_s(buffer, "%s:%i\n%.*s", file, line, length, errorDesc);

        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
        MessageBoxA(NULL, buffer, "ERROR", MB_ICONERROR | MB_OK);
    }
};

RendererErrorCallback g_ErrorCallback;

HRESULT CreateVXGIObject()
{
    VXGI::GIParameters params;
    params.rendererInterface = g_pRendererInterface;
    params.errorCallback = &g_ErrorCallback;

    VXGI::ShaderCompilerParameters comparams;
    comparams.errorCallback = &g_ErrorCallback;
    comparams.graphicsAPI = g_pRendererInterface->getGraphicsAPI();
    // comparams.d3dCompilerDLLName = "d3dcompiler_hook.dll";
#ifndef NDEBUG
    comparams.d3dCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    comparams.d3dCompileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    comparams.d3dCompileFlags2 = 0U;

    if (VXGI_FAILED(VFX_VXGI_CreateShaderCompiler(comparams, &g_pGICompiler)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI shader compiler.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    if (VXGI_FAILED(VFX_VXGI_CreateGIObject(params, &g_pGI)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI object.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    // Custom G-buffer Support
    {
        VXGI::ShaderResources resources;
        resources.constantBufferCount = 1U;
        resources.constantBufferSlots[0] = CBV_SLOT_BUILTIN_GBUFFER_PARAMETERS;
        resources.textureCount = 4U;
        resources.textureSlots[0] = SRV_SLOT_DEPTH_BUFFER;
        resources.textureSlots[1] = SRV_SLOT_DEPTH_BUFFER_PREV;
        resources.textureSlots[2] = SRV_SLOT_NORMAL_BUFFER;
        resources.textureSlots[3] = SRV_SLOT_NORMAL_BUFFER_PREV;
        resources.samplerCount = 0;

        VXGI::IBlob *blob = nullptr;

        if (VXGI_FAILED(g_pGICompiler->compileViewTracingShaders(&blob, g_GBufferLoader, sizeof(g_GBufferLoader), resources)))
            return E_FAIL;

        if (VXGI_FAILED(g_pGI->loadUserDefinedShaderSet(&g_GIGBufferLoader, blob->getData(), blob->getSize())))
            return E_FAIL;

        blob->dispose();
    }

    g_pBuiltinGBufferParameters = g_pRendererInterface->createConstantBuffer(NVRHI::ConstantBufferDesc(sizeof(BuiltinGBufferParameters), nullptr), nullptr);

    if (VXGI_FAILED(g_pGI->createCustomTracer(&g_pGITracer, g_GIGBufferLoader)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI tracer.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    VXGI::VoxelizationParameters voxelizationParams;
    voxelizationParams.ambientOcclusionMode = true;
    voxelizationParams.mapSize = VXGI::uint3(g_nMapSize);
    voxelizationParams.stackLevels = BRX_VCT_CLIPMAP_STACK_LEVEL_COUNT;
    voxelizationParams.mipLevels = BRX_VCT_CLIPMAP_MIP_LEVEL_COUNT;
    // The **VXGI::VoxelizationParameters::persistentVoxelData** is always set to **false** in the **NVIDIA Unreal Engine 4 Fork**.
    voxelizationParams.persistentVoxelData = false;
    voxelizationParams.enabledHardwareFeatures = VXGI::HardwareFeatures::TYPED_UAV_LOAD;

    if (VXGI_FAILED(g_pGI->setVoxelizationParameters(voxelizationParams)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to initialize VXGI voxelization.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    return S_OK;
}

class AntTweakBarVisualController : public IVisualController
{
    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        if (g_bRenderHUD || uMsg == WM_KEYDOWN || uMsg == WM_CHAR)
            if (TwEventWin(hWnd, uMsg, wParam, lParam))
            {
                return 0; // Event has been handled by AntTweakBar
            }

        return 1;
    }

    void RenderText()
    {
        TwBeginText(2, 0, 0, 0);
        const unsigned int color = 0xffffc0ff;
        char msg[256];

        double averageTime = g_DeviceManager->GetAverageFrameTime();
        double fps = (averageTime > 0) ? 1.0 / averageTime : 0.0;

        sprintf_s(msg, "%.1f FPS", fps);
        TwAddTextLine(msg, color, 0);

        TwEndText();
    }

    virtual void Render(RenderTargetView RTV) override
    {
        g_pRendererInterface->debugBeginEvent("AntTweakBar");

#if USE_D3D12
        TwSetD3D12RenderTargetView((void *)RTV.ptr);
#else
        (void)RTV;
#endif

        if (g_bRenderHUD)
        {
            RenderText();
            TwDraw();
        }

        g_pRendererInterface->debugEndEvent();
    }

    virtual HRESULT DeviceCreated() override
    {
#if USE_D3D11
        TwInit(TW_DIRECT3D11, g_DeviceManager->GetDevice());
#elif USE_D3D12
        TwD3D12DeviceInfo info;
        info.Device = g_DeviceManager->GetDevice();
        info.DefaultCommandQueue = g_DeviceManager->GetDefaultQueue();
        info.RenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        info.RenderTargetSampleCount = 1;
        info.RenderTargetSampleQuality = 0;
        info.UploadBufferSize = 1024 * 1024;
        TwInit(TW_DIRECT3D12, &info);
#endif
        InitDialogs();
        return S_OK;
    }

    virtual void DeviceDestroyed() override
    {
        TwTerminate();
    }

    virtual void BackBufferResized(uint32_t width, uint32_t height, uint32_t sampleCount) override
    {
        (void)sampleCount;
        TwWindowSize(width, height);
    }

    void InitDialogs()
    {
        TwBar *bar = TwNewBar("barMain");
        TwDefine("barMain label='Settings' size='250 220' valueswidth=80");

        TwAddVarRW(bar, "Show AO Channel", TW_TYPE_BOOLCPP, &g_bVisualizeAO, "");
        TwAddSeparator(bar, nullptr, nullptr);
        TwAddVarRW(bar, "Enable VXAO", TW_TYPE_BOOLCPP, &g_bEnableVXAO, "");
        TwAddVarRW(bar, "VXAO Range", TW_TYPE_FLOAT, &g_fVxaoRange, "min=100 max=1000 step=1");
        TwAddVarRW(bar, "VXAO Quality", TW_TYPE_FLOAT, &g_fVxaoQuality, "min=0 max=1 step=0.01");
        TwAddVarRW(bar, "VXAO Sampling Rate", TW_TYPE_FLOAT, &g_fVxaoSamplingRate, "min=0.25 max=1 step=0.01");
        TwAddVarRW(bar, "VXAO Scale", TW_TYPE_FLOAT, &g_fVxaoScale, "min=0.25 max=2 step=0.01");
        TwAddVarRW(bar, "Temporal Filtering", TW_TYPE_BOOLCPP, &g_bTemporalFiltering, nullptr);
        TwAddVarRW(bar, "Show Voxels", TW_TYPE_BOOLCPP, &g_bShowVoxels, "");
    }
};

class MainVisualController : public IVisualController
{
    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);

        if (uMsg == WM_KEYDOWN)
        {
            int iKeyPressed = static_cast<int>(wParam);

            switch (iKeyPressed)
            {
            case VK_TAB:
                g_bRenderHUD = !g_bRenderHUD;
                return 0;
                break;
            }
        }

        return 1;
    }

    virtual void Animate(double fElapsedTimeSeconds) override
    {
        g_Camera.FrameMove((float)fElapsedTimeSeconds);
    }

    virtual void Render(RenderTargetView RTV) override
    {
#if USE_D3D11
        ID3D11Resource *pMainResource = NULL;
        RTV->GetResource(&pMainResource);
        NVRHI::TextureHandle mainRenderTarget = g_pRendererInterface->getHandleForTexture(pMainResource);
        pMainResource->Release();
#elif USE_D3D12
        (void)RTV;
        NVRHI::TextureHandle mainRenderTarget = g_pRendererInterface->getHandleForTexture(g_DeviceManager->GetCurrentBackBuffer());
        g_pRendererInterface->setNonManagedTextureResourceState(mainRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
#endif

        XMVECTOR eyePt = g_Camera.GetEyePt();
        XMVECTOR viewForward = g_Camera.GetWorldAhead();
        XMMATRIX viewMatrix = g_Camera.GetViewMatrix();
        XMMATRIX projMatrix = g_Camera.GetProjMatrix();
        XMMATRIX viewProjMatrix = viewMatrix * projMatrix;

        if (g_bEnableVXAO)
        {
            DirectX::XMFLOAT3 clipmap_anchor;
            {
                XMVECTOR centerPt = eyePt + viewForward * g_fVoxelSize * float(g_nMapSize) * 0.5f;
                DirectX::XMStoreFloat3(&clipmap_anchor, centerPt);
            }

            VXGI::UpdateVoxelizationParameters params;
            params.clipmapAnchor.x = clipmap_anchor.x;
            params.clipmapAnchor.y = clipmap_anchor.y;
            params.clipmapAnchor.z = clipmap_anchor.z;
            params.finestVoxelSize = g_fVoxelSize;

            bool performOpacityVoxelization = false;
            bool performEmittanceVoxelization = false;
            {
                g_pRendererInterface->debugBeginEvent("VXGI Toroidal Addressing (Persistent Voxel Data)");

                g_pGI->prepareForVoxelization(params, performOpacityVoxelization, performEmittanceVoxelization);

                g_pRendererInterface->debugEndEvent();
            }

            FLUSH_COMMAND_LIST;

            if (performOpacityVoxelization)
            {
                VXGI::VoxelizationViewParameters viewParams;
                g_pGI->getVoxelizationViewParameters(viewParams);
                VXGI::float4x4 voxelizationMatrix = viewParams.viewMatrix * viewParams.projectionMatrix;

                const uint32_t maxRegions = 128;
                uint32_t numRegions = 0;
                VXGI::Box3f regions[maxRegions];

                if (VXGI_SUCCEEDED(g_pGI->getInvalidatedRegions(regions, maxRegions, numRegions)))
                {
                    g_pRendererInterface->debugBeginEvent("VXGI Light Injection (Voxelization Opacity Geometry)");

                    NVRHI::DrawCallState emptyState;
                    g_pSceneRenderer->RenderForVoxelization(emptyState, g_pGI, regions, numRegions, clipmap_anchor, voxelizationMatrix, NULL);

                    g_pRendererInterface->debugEndEvent();
                }
            }

            FLUSH_COMMAND_LIST;

            g_pRendererInterface->debugBeginEvent("VXGI Filtering");

            g_pGI->finalizeVoxelization();

            g_pRendererInterface->debugEndEvent();
        }

        {
            g_pRendererInterface->debugBeginEvent("Base Pass");

            g_pSceneRenderer->RenderToGBuffer((VXGI::float4x4 &)viewProjMatrix);

            g_pRendererInterface->debugEndEvent();
        }

        VXGI::IBasicViewTracer::InputBuffers inputBuffers;
        g_pSceneRenderer->FillTracingInputBuffers(inputBuffers);
        memcpy(&inputBuffers.viewMatrix, &viewMatrix, sizeof(viewMatrix));
        memcpy(&inputBuffers.projMatrix, &projMatrix, sizeof(projMatrix));

        // Custom GBuffer Loader
        BuiltinGBufferParameters builtinGBufferParameters;
        builtinGBufferParameters.g_GBuffer.projMatrixInv = inputBuffers.projMatrix.invert();
        builtinGBufferParameters.g_GBuffer.viewMatrixInv = inputBuffers.viewMatrix.invert();
        builtinGBufferParameters.g_GBuffer.viewportOrigin = VXGI::float2(inputBuffers.gbufferViewport.minX, inputBuffers.gbufferViewport.minY);
        builtinGBufferParameters.g_GBuffer.viewportSizeInv = VXGI::float2(1.0F / (inputBuffers.gbufferViewport.maxX - inputBuffers.gbufferViewport.minX), 1.0F / (inputBuffers.gbufferViewport.maxY - inputBuffers.gbufferViewport.minY));
        builtinGBufferParameters.g_PreviousGBuffer.projMatrixInv = g_InputBuffersPrev.projMatrix.invert();
        builtinGBufferParameters.g_PreviousGBuffer.viewMatrixInv = g_InputBuffersPrev.viewMatrix.invert();
        builtinGBufferParameters.g_PreviousGBuffer.viewportOrigin = VXGI::float2(g_InputBuffersPrev.gbufferViewport.minX, g_InputBuffersPrev.gbufferViewport.minY);
        builtinGBufferParameters.g_PreviousGBuffer.viewportSizeInv = VXGI::float2(1.0F / (g_InputBuffersPrev.gbufferViewport.maxX - g_InputBuffersPrev.gbufferViewport.minX), 1.0F / (g_InputBuffersPrev.gbufferViewport.maxY - g_InputBuffersPrev.gbufferViewport.minY));
        g_pRendererInterface->writeConstantBuffer(g_pBuiltinGBufferParameters, &builtinGBufferParameters, sizeof(BuiltinGBufferParameters));

        NVRHI::PipelineStageBindings gbufferBindings;
        NVRHI::BindConstantBuffer(gbufferBindings, CBV_SLOT_BUILTIN_GBUFFER_PARAMETERS, g_pBuiltinGBufferParameters);
        NVRHI::BindTexture(gbufferBindings, SRV_SLOT_DEPTH_BUFFER, inputBuffers.gbufferDepth, false, NVRHI::Format::UNKNOWN, 0U);
        NVRHI::BindTexture(gbufferBindings, SRV_SLOT_DEPTH_BUFFER_PREV, g_InputBuffersPrev.gbufferDepth, false, NVRHI::Format::UNKNOWN, 0U);
        NVRHI::BindTexture(gbufferBindings, SRV_SLOT_NORMAL_BUFFER, inputBuffers.gbufferNormal, false, NVRHI::Format::UNKNOWN, 0U);
        NVRHI::BindTexture(gbufferBindings, SRV_SLOT_NORMAL_BUFFER_PREV, g_InputBuffersPrev.gbufferNormal, false, NVRHI::Format::UNKNOWN, 0U);

        VXGI::int2 gbufferSize(static_cast<int>(inputBuffers.gbufferViewport.maxX - inputBuffers.gbufferViewport.minX), static_cast<int>(inputBuffers.gbufferViewport.maxY - inputBuffers.gbufferViewport.minY));

        VXGI::IViewTracer::ViewInfo views[1];
        views[0].extents.minX = static_cast<int>(inputBuffers.gbufferViewport.minX);
        views[0].extents.maxX = static_cast<int>(inputBuffers.gbufferViewport.maxX);
        views[0].extents.minY = static_cast<int>(inputBuffers.gbufferViewport.minY);
        views[0].extents.maxY = static_cast<int>(inputBuffers.gbufferViewport.maxY);
        views[0].matricesAreValid = true;
        views[0].reverseProjection = false; // TODO:?
        views[0].viewMatrix = inputBuffers.viewMatrix;
        views[0].projMatrix = inputBuffers.projMatrix;

        NVRHI::TextureHandle gbufferAlbedo = g_pSceneRenderer->GetAlbedoBufferHandle();

        if (g_bShowVoxels)
        {
            VXGI::DebugRenderParameters params;
            params.debugMode = VXGI::DebugRenderMode::OPACITY_TEXTURE;
            params.viewMatrix = *(VXGI::float4x4 *)&viewMatrix;
            params.projMatrix = *(VXGI::float4x4 *)&projMatrix;
            params.viewport = inputBuffers.gbufferViewport;
            params.destinationTexture = gbufferAlbedo;
            params.destinationDepth = inputBuffers.gbufferDepth;
            params.level = -1;
            params.blendState.blendEnable[0] = true;
            params.blendState.srcBlend[0] = NVRHI::BlendState::BLEND_SRC_ALPHA;
            params.blendState.destBlend[0] = NVRHI::BlendState::BLEND_INV_SRC_ALPHA;

            g_pGI->renderDebug(params);

            g_pSceneRenderer->Blit(gbufferAlbedo, mainRenderTarget);
        }
        else
        {
            if (g_bEnableVXAO)
            {
                NVRHI::TextureHandle vxao = NULL;
                NVRHI::TextureHandle confidence = NULL;
                {
                    g_pGITracer->beginFrame();

                    g_pRendererInterface->debugBeginEvent("VXGI Diffuse Cone Tracing");

                    VXGI::BasicDiffuseTracingParameters diffuseParams;
                    diffuseParams.quality = g_fVxaoQuality;
                    diffuseParams.directionalSamplingRate = g_fVxaoSamplingRate;
                    diffuseParams.ambientRange = g_fVxaoRange;
                    diffuseParams.interpolationWeightThreshold = 0.1f;
                    diffuseParams.ambientScale = g_fVxaoScale;
                    diffuseParams.enableTemporalReprojection = g_bTemporalFiltering;
                    diffuseParams.enableTemporalJitter = g_bTemporalFiltering;

                    g_pGITracer->computeDiffuseChannel(diffuseParams, gbufferBindings, gbufferSize, views, 1U, vxao, confidence);

                    g_pRendererInterface->debugEndEvent();
                }

                {
                    g_pRendererInterface->debugBeginEvent("Deferred Lighting");

                    g_pSceneRenderer->ComposeAO(vxao, mainRenderTarget, g_bVisualizeAO);

                    g_pRendererInterface->debugEndEvent();
                }
            }
            else
            {
                g_pSceneRenderer->Blit(gbufferAlbedo, mainRenderTarget);
            }
        }

        g_InputBuffersPrev = inputBuffers;
        g_InputBuffersPrevValid = true;

#if USE_D3D11
        g_pRendererInterface->forgetAboutTexture(pMainResource);
#elif USE_D3D12
        // This needs to be done before resizing the window, but there's no PreResize event from DeviceManager
        g_pRendererInterface->releaseNonManagedTextures();

        g_pRendererInterface->flushCommandList();
#endif
    }

    virtual HRESULT DeviceCreated() override
    {
#if USE_D3D11
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D11(&g_ErrorCallback, g_DeviceManager->GetImmediateContext());
#elif USE_D3D12
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D12(&g_ErrorCallback, g_DeviceManager->GetDevice(), g_DeviceManager->GetDefaultQueue());
#endif
        g_pSceneRenderer = new SceneRenderer(g_pRendererInterface);

        if (FAILED(CreateVXGIObject()))
            return E_FAIL;

        char strFileName[512];
        if (FAILED(DXUTFindDXSDKMediaFileCch(strFileName, 512, "thirdparty\\sponza\\sponza.gltf")))
            return E_FAIL;

        if (FAILED(g_pSceneRenderer->LoadMesh(strFileName)))
            return E_FAIL;

        if (FAILED(g_pSceneRenderer->AllocateResources(g_pGI, g_pGICompiler)))
            return E_FAIL;

        g_bInitialized = true;

        return S_OK;
    }

    virtual void DeviceDestroyed() override
    {
        if (g_pSceneRenderer)
        {
            g_pSceneRenderer->ReleaseViewDependentResources();
            g_pSceneRenderer->ReleaseResources(g_pGI);
        }

        if (g_pGI)
        {
            if (g_pGITracer)
            {
                g_pGI->destroyTracer(g_pGITracer);
                g_pGITracer = NULL;
            }

            if (g_GIGBufferLoader)
            {
                g_pGI->destroyUserDefinedShaderSet(g_GIGBufferLoader);
                g_GIGBufferLoader = NULL;
            }

            VFX_VXGI_DestroyGIObject(g_pGI);
            g_pGI = NULL;
        }

        if (g_pSceneRenderer)
        {
            delete g_pSceneRenderer;
            g_pSceneRenderer = NULL;
        }
    }

    virtual void BackBufferResized(uint32_t width, uint32_t height, uint32_t sampleCount) override
    {
        g_InputBuffersPrevValid = false;

        g_pSceneRenderer->ReleaseViewDependentResources();

        g_pSceneRenderer->AllocateViewDependentResources(width, height, sampleCount);

        // Setup the camera's projection parameters
        float fAspectRatio = width / (FLOAT)height;
        g_Camera.SetProjParams(XM_PIDIV4, fAspectRatio, g_fCameraClipNear, g_fCameraClipFar);
    }
};

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    g_DeviceManager = new DeviceManager();

    MainVisualController sceneController;
    AntTweakBarVisualController atbController;
    g_DeviceManager->AddControllerToFront(&sceneController);
    g_DeviceManager->AddControllerToFront(&atbController);

    DeviceCreationParameters deviceParams;
    deviceParams.backBufferWidth = 1280;
    deviceParams.backBufferHeight = 720;
    deviceParams.swapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    deviceParams.swapChainSampleCount = 1;
    deviceParams.swapChainBufferCount = 4;
    deviceParams.startFullscreen = false;
#ifdef DEBUG
    deviceParams.enableDebugRuntime = true;
#endif

    if (FAILED(g_DeviceManager->CreateWindowDeviceAndSwapChain(deviceParams, L"VXGI Sample: Ambient Occlusion (" API_STRING ")")))
    {
        MessageBox(NULL, L"Cannot initialize the " API_STRING " device with the requested parameters", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // https://github.com/KhronosGroup/glTF-Sample-Models/blob/main/2.0/Sponza/glTF/Sponza.gltf#L8558
    // https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Sponza/glTF/Sponza.gltf#L8558
    DirectX::XMVECTOR const offset = DirectX::XMVectorSet(0.0F, 0.0F, 0.0F, 0.0F); // DirectX::XMVectorSet(60.5189208984375F, 126.44249725341797F, 38.690551757812F, 0.0F);
    constexpr float const scale = 0.00800000037997961F;

    DirectX::XMVECTOR eyePt = DirectX::XMVectorAdd(DirectX::XMVectorSet(0.0F, 100.0F, 50.0F, 0.0F), offset);
    DirectX::XMVECTOR lookAtPt = DirectX::XMVectorAdd(DirectX::XMVectorSet(-100.0F, 100.0F, 50.0F, 0.0F), offset);
    g_Camera.SetViewParams(eyePt, lookAtPt);
    g_Camera.SetScalers(0.005F, 500.0F);
    g_Camera.SetRotateButtons(true, false, false, false);

    if (g_bInitialized)
        g_DeviceManager->MessageLoop();

    g_DeviceManager->Shutdown();
    delete g_DeviceManager;

    return 0;
}
