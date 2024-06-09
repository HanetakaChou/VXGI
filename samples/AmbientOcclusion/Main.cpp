/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "SceneRenderer.h"
#include "Camera.h"
#include "SDKmisc.h"
#include <AntTweakBar.h>

#if USE_D3D11

#include "DeviceManager11.h"
#include "GFSDK_NVRHI_D3D11.h"
#define API_STRING "D3D11"
NVRHI::RendererInterfaceD3D11 *g_pRendererInterface = NULL;

#elif USE_D3D12

#include "DeviceManager12.h"
#include "GFSDK_NVRHI_D3D12.h"
#define API_STRING "D3D12"
NVRHI::RendererInterfaceD3D12 *g_pRendererInterface = NULL;

#elif USE_GL4

#include "DeviceManagerGL4.h"
#include "GFSDK_NVRHI_OpenGL4.h"
#define API_STRING "OpenGL"
NVRHI::RendererInterfaceOGL *g_pRendererInterface = NULL;

#endif

using namespace DirectX;

CFirstPersonCamera g_Camera;
DeviceManager *g_DeviceManager = NULL;
SceneRenderer *g_pSceneRenderer = NULL;
VXGI::IGlobalIllumination *g_pGI = NULL;
VXGI::IShaderCompiler *g_pGICompiler = NULL;
VXGI::IViewTracer *g_pGITracer = NULL;

static float g_fCameraClipNear = 1.0f;
static float g_fCameraClipFar = 10000.0f;
static float g_fClipmapRange = 512.0f;
static float g_fAmbientRange = 512.0f;
static bool g_bEnableVXAO = true;
static bool g_bVisualizeAO = false;
static bool g_bRenderHUD = true;
static VXGI::DebugRenderMode::Enum g_DebugRenderMode = VXGI::DebugRenderMode::DISABLED;
static int g_iDebugLevel = 0;
static bool g_bInitialized = false;
static int g_iTracingSparsity = 4;
static int g_iNumCones = 8;
static bool g_bUseRefinement = true;
static bool g_bEnableSSAO = false;

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
    comparams.d3dCompilerDLLName = "d3dcompiler_hook.dll";

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

    if (VXGI_FAILED(g_pGI->createNewTracer(&g_pGITracer)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI tracer.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    VXGI::VoxelizationParameters voxelizationParams;
    voxelizationParams.emittanceFormat = VXGI::EmittanceFormat::NONE; // Enable VXAO mode
    voxelizationParams.opacityDirectionCount = VXGI::OpacityDirections::THREE_DIMENSIONAL;
    voxelizationParams.mapSize = 128;
    // The **VXGI::VoxelizationParameters::persistentVoxelData** is always set to **false** in the **NVIDIA Unreal Engine 4 Fork**.
    voxelizationParams.persistentVoxelData = false;
    voxelizationParams.enableNvidiaExtensions = false;
    voxelizationParams.enableGeometryShaderPassthrough = false;

    if (VXGI_FAILED(g_pGI->setVoxelizationParameters(voxelizationParams)))
    {
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
#elif USE_GL4
        TwInit(TW_OPENGL_CORE, nullptr);
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
        TwDefine("barMain label='Settings' size='250 200' valueswidth=100");

        TwAddVarRW(bar, "Enable VXAO", TW_TYPE_BOOLCPP, &g_bEnableVXAO, "");
        TwAddVarRW(bar, "Visualize AO", TW_TYPE_BOOLCPP, &g_bVisualizeAO, "");
        TwAddVarRW(bar, "Ambient range", TW_TYPE_FLOAT, &g_fAmbientRange, "min=10 max=10000 step=1");
        TwAddVarRW(bar, "Number of cones", TW_TYPE_INT32, &g_iNumCones, "min=4 max=32");
        TwAddVarRW(bar, "Tracing sparsity", TW_TYPE_INT32, &g_iTracingSparsity, "min=1 max=4");
        TwAddVarRW(bar, "Refine sparse tracing", TW_TYPE_BOOLCPP, &g_bUseRefinement, "");
        TwAddVarRW(bar, "Use buislt-in SSAO", TW_TYPE_BOOLCPP, &g_bEnableSSAO, "");

        { // Debug rendering mode
            TwEnumVal debugRenderModeEV[] = {
                {VXGI::DebugRenderMode::DISABLED, "No debug rendering"},
                {VXGI::DebugRenderMode::ALLOCATION_MAP, "Allocation map"},
                {VXGI::DebugRenderMode::OPACITY_TEXTURE, "Opacity map"},
            };
            TwType debugRenderModeType = TwDefineEnum("Debug rendering mode", debugRenderModeEV, sizeof(debugRenderModeEV) / sizeof(debugRenderModeEV[0]));
            TwAddVarRW(bar, "Debug mode", debugRenderModeType, &g_DebugRenderMode, "keyIncr=g keyDecr=G");
        }

        TwAddVarRW(bar, "Debug level", TW_TYPE_INT32, &g_iDebugLevel, "min=0 max=3");
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

            case VK_SPACE:
                g_DebugRenderMode = VXGI::DebugRenderMode::DISABLED;
                return 0;
                break;
            }

            if (g_DebugRenderMode != VXGI::DebugRenderMode::DISABLED && iKeyPressed >= '1' && iKeyPressed <= '9')
            {
                g_iDebugLevel = iKeyPressed - '1';
                return 0;
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
#elif USE_GL4
        (void)RTV;
        NVRHI::TextureHandle mainRenderTarget = g_pRendererInterface->getHandleForDefaultBackBuffer();
#endif

        XMVECTOR eyePt = g_Camera.GetEyePt();
        XMVECTOR viewForward = g_Camera.GetWorldAhead();
        XMMATRIX viewMatrix = g_Camera.GetViewMatrix();
        XMMATRIX projMatrix = g_Camera.GetProjMatrix();
        XMMATRIX viewProjMatrix = viewMatrix * projMatrix;

        if (g_bEnableVXAO)
        {
            XMVECTOR centerPt = eyePt + viewForward * g_fClipmapRange;

            VXGI::UpdateVoxelizationParameters params;
            params.clipmapAnchor = VXGI::Vector3f(centerPt.m128_f32);
            params.giRange = g_fClipmapRange;

            bool performOpacityVoxelization = false;
            bool performEmittanceVoxelization = false;

            g_pRendererInterface->debugBeginEvent("VXGI Toroidal Addressing (Persistent Voxel Data)");

            g_pGI->prepareForOpacityVoxelization(params, performOpacityVoxelization, performEmittanceVoxelization);

            g_pRendererInterface->debugEndEvent();

            if (performOpacityVoxelization)
            {
                VXGI::Matrix4f voxelizationMatrix;
                g_pGI->getVoxelizationViewMatrix(voxelizationMatrix);

                const uint32_t maxRegions = 128;
                uint32_t numRegions = 0;
                VXGI::Box3f regions[maxRegions];

                if (VXGI_SUCCEEDED(g_pGI->getInvalidatedRegions(regions, maxRegions, numRegions)))
                {
                    g_pRendererInterface->debugBeginEvent("VXGI Light Injection (Generate Opacity Texture) (Voxelization Opacity Geometry)");

                    NVRHI::DrawCallState emptyState;
                    g_pSceneRenderer->RenderSceneCommon(emptyState, g_pGI, regions, numRegions, voxelizationMatrix, NULL, true);

                    g_pRendererInterface->debugEndEvent();
                }
            }

            g_pRendererInterface->debugBeginEvent("VXGI Filtering");

            g_pGI->finalizeVoxelization();

            g_pRendererInterface->debugEndEvent();
        }

        {
            g_pRendererInterface->debugBeginEvent("Base Pass");

            g_pSceneRenderer->RenderToGBuffer((VXGI::Matrix4f &)viewProjMatrix);

            g_pRendererInterface->debugEndEvent();
        }

        VXGI::IViewTracer::InputBuffers inputBuffers;
        g_pSceneRenderer->FillTracingInputBuffers(inputBuffers);
        memcpy(&inputBuffers.viewMatrix, &viewMatrix, sizeof(viewMatrix));
        memcpy(&inputBuffers.projMatrix, &projMatrix, sizeof(projMatrix));

        NVRHI::TextureHandle gbufferAlbedo = g_pSceneRenderer->GetAlbedoBufferHandle();

        if (g_DebugRenderMode != VXGI::DebugRenderMode::DISABLED)
        {
            VXGI::DebugRenderParameters params;
            params.debugMode = g_DebugRenderMode;
            params.viewMatrix = *(VXGI::Matrix4f *)&viewMatrix;
            params.projMatrix = *(VXGI::Matrix4f *)&projMatrix;
            params.viewport = inputBuffers.gbufferViewport;
            params.destinationTexture = gbufferAlbedo;
            params.destinationDepth = inputBuffers.gbufferDepth;
            params.level = g_iDebugLevel;
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
                // VXGI Lighting
                NVRHI::TextureHandle ambientOcclusion = NULL;
                {
                    VXGI::DiffuseTracingParameters diffuseParams;
                    diffuseParams.numCones = g_iNumCones;
                    diffuseParams.tracingSparsity = g_iTracingSparsity;
                    diffuseParams.ambientRange = g_fAmbientRange;
                    diffuseParams.enableSparseTracingRefinement = g_bUseRefinement;
                    diffuseParams.enableSSAO = g_bEnableSSAO;

                    g_pRendererInterface->debugBeginEvent("VXGI Diffuse Cone Tracing");

                    g_pGITracer->computeDiffuseChannel(diffuseParams, ambientOcclusion, inputBuffers);

                    g_pRendererInterface->debugEndEvent();
                }

                {
                    g_pRendererInterface->debugBeginEvent("Deferred Lighting");

                    if (g_bVisualizeAO)
                    {
                        g_pSceneRenderer->Blit(ambientOcclusion, mainRenderTarget);
                    }
                    else
                    {
                        g_pSceneRenderer->ComposeAO(ambientOcclusion, mainRenderTarget);
                    }

                    g_pRendererInterface->debugEndEvent();
                }
            }
            else
            {
                g_pSceneRenderer->Blit(gbufferAlbedo, mainRenderTarget);
            }
        }

#if USE_D3D11
        g_pRendererInterface->forgetAboutTexture(pMainResource);
#elif USE_D3D12
        // This needs to be done before resizing the window, but there's no PreResize event from DeviceManager
        g_pRendererInterface->releaseNonManagedTextures();

        g_pRendererInterface->flushCommandList();
#elif USE_GL4
        g_pRendererInterface->UnbindFrameBuffer();
#endif
    }

    virtual HRESULT DeviceCreated() override
    {
#if USE_D3D11
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D11(&g_ErrorCallback, g_DeviceManager->GetImmediateContext());
#elif USE_D3D12
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D12(&g_ErrorCallback, g_DeviceManager->GetDevice(), g_DeviceManager->GetDefaultQueue());
#elif USE_GL4
        g_pRendererInterface = new NVRHI::RendererInterfaceOGL(&g_ErrorCallback);
        g_pRendererInterface->init();
#endif
        g_pSceneRenderer = new SceneRenderer(g_pRendererInterface);

        if (FAILED(CreateVXGIObject()))
            return E_FAIL;

        char strFileName[512];
        if (FAILED(DXUTFindDXSDKMediaFileCch(strFileName, 512, "Sponza\\SponzaNoFlag.obj")))
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
                g_pGI->destroyTracer(g_pGITracer);
            g_pGITracer = NULL;

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
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_DeviceManager = new DeviceManager();

    MainVisualController sceneController;
    AntTweakBarVisualController atbController;
    g_DeviceManager->AddControllerToFront(&sceneController);
    g_DeviceManager->AddControllerToFront(&atbController);

    DeviceCreationParameters deviceParams;
    deviceParams.backBufferWidth = 1280;
    deviceParams.backBufferHeight = 720;
#if !USE_GL4
    deviceParams.swapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    deviceParams.swapChainSampleCount = 1;
    deviceParams.swapChainBufferCount = 4;
    deviceParams.startFullscreen = false;
#endif
#ifdef DEBUG
    deviceParams.enableDebugRuntime = true;
#endif

    if (FAILED(g_DeviceManager->CreateWindowDeviceAndSwapChain(deviceParams, L"VXGI Sample: Ambient Occlusion (" API_STRING ")")))
    {
        MessageBox(NULL, L"Cannot initialize the " API_STRING " device with the requested parameters", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    XMVECTOR eyePt = XMVectorSet(0.0f, 100.0f, 50.0f, 0);
    XMVECTOR lookAtPt = XMVectorSet(-100.0f, 100.0f, 50.0f, 0);
    g_Camera.SetViewParams(eyePt, lookAtPt);
    g_Camera.SetScalers(0.005f, 500.0f);
    g_Camera.SetRotateButtons(true, false, false, false);

    if (g_bInitialized)
        g_DeviceManager->MessageLoop();

    g_DeviceManager->Shutdown();
    delete g_DeviceManager;

    return 0;
}