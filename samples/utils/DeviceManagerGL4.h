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
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <sdkddkver.h>
#include <Windows.h>
#include <list>
#include <string>

#include <glad/glad_wgl.h>
#include <glad/glad.h>

struct DeviceCreationParameters
{
    int backBufferWidth;
    int backBufferHeight;
    bool enableDebugRuntime;

    DeviceCreationParameters()
        : backBufferWidth(1280), backBufferHeight(720), enableDebugRuntime(false)
    {
    }
};

typedef uint32_t RenderTargetView;

#pragma warning(push)
#pragma warning(disable : 4100) // unreferenced formal parameter
class IVisualController
{
private:
    bool m_Enabled;

public:
    IVisualController() : m_Enabled(true) {}

    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return 1; }
    virtual void Render(RenderTargetView RTV) {}
    virtual void Animate(double fElapsedTimeSeconds) {}
    virtual HRESULT DeviceCreated() { return S_OK; }
    virtual void DeviceDestroyed() {}
    virtual void BackBufferResized(uint32_t width, uint32_t height, uint32_t sampleCount) {}

    virtual void EnableController() { m_Enabled = true; }
    virtual void DisableController() { m_Enabled = false; }
    virtual bool IsEnabled() { return m_Enabled; }
};
#pragma warning(pop)

class DeviceManager
{
public:
    enum WindowState
    {
        kWindowNone,
        kWindowNormal,
        kWindowMinimized,
        kWindowMaximized,
        kWindowFullscreen
    };

protected:
    HDC m_DC;
    HGLRC m_GLcontext;
    bool m_IsNvidia;
    HWND m_hWnd;
    std::list<IVisualController *> m_vControllers;
    std::wstring m_WindowTitle;
    double m_FixedFrameInterval;
    UINT m_SyncInterval;
    std::list<double> m_vFrameTimes;
    double m_AverageFrameTime;
    double m_AverageTimeUpdateInterval;
    bool m_InSizingModalLoop;
    SIZE m_WindowSize;
    bool m_ShutdownCalled;
    bool m_EnableRenderTargetClear;
    float m_RenderTargetClearColor[4];

public:
    DeviceManager()
        : m_DC(0), m_GLcontext(0), m_IsNvidia(false), m_hWnd(NULL), m_WindowTitle(L""), m_FixedFrameInterval(-1), m_SyncInterval(0), m_AverageFrameTime(0), m_AverageTimeUpdateInterval(0.5), m_InSizingModalLoop(false), m_ShutdownCalled(false)
    {
        m_WindowSize = {};
    }

    virtual ~DeviceManager()
    {
        Shutdown();
    }

    virtual HRESULT CreateWindowDeviceAndSwapChain(const DeviceCreationParameters &params, LPWSTR windowTitle);
    virtual HRESULT ResizeWindow(int width, int height);

    virtual void Shutdown();
    virtual void MessageLoop();
    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Render();
    virtual void Animate(double fElapsedTimeSeconds);
    virtual void DeviceCreated();
    virtual void DeviceDestroyed();
    virtual void BackBufferResized();

    void AddControllerToFront(IVisualController *pController);
    void AddControllerToBack(IVisualController *pController);
    void RemoveController(IVisualController *pController);

    void SetFixedFrameInterval(double seconds) { m_FixedFrameInterval = seconds; }
    void DisableFixedFrameInterval() { m_FixedFrameInterval = -1; }

    bool IsNvidia() const { return m_IsNvidia; }
    HWND GetHWND() { return m_hWnd; }
    WindowState GetWindowState();
    HGLRC GetGLContext() const { return m_GLcontext; }
    bool GetVsyncEnabled() { return m_SyncInterval > 0; }
    void SetVsyncEnabled(bool enabled) { m_SyncInterval = enabled ? 1 : 0; }
    HRESULT GetDisplayResolution(int &width, int &height);
    double GetAverageFrameTime() { return m_AverageFrameTime; }
    void SetAverageTimeUpdateInterval(double value) { m_AverageTimeUpdateInterval = value; }
    void SetPrimaryRenderTargetClearColor(bool enableClear, const float *pColor);
};
