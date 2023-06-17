/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "DeviceManagerGL4.h"
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <sdkddkver.h>
#include <Windows.h>
#include <assert.h>
#include <sstream>
#include <algorithm>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)     \
    {                       \
        if (p)              \
        {                   \
            (p)->Release(); \
            (p) = NULL;     \
        }                   \
    }
#endif

extern "C" IMAGE_DOS_HEADER __ImageBase;
static HINSTANCE hInstance_OpenGL32;
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void *GladLoadProc(const char *name);
static void GLAPIENTRY GLDebugMessageProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);

HRESULT DeviceManager::CreateWindowDeviceAndSwapChain(const DeviceCreationParameters &params, LPWSTR title)
{
    hInstance_OpenGL32 = LoadLibraryW(L"OpenGL32.dll");
    assert(NULL != hInstance_OpenGL32);

    m_WindowTitle = title;

    HINSTANCE hInstance = ((HINSTANCE)&__ImageBase);

    WNDCLASSEXW windowClass = {
        sizeof(WNDCLASSEXW),
        CS_HREDRAW | CS_VREDRAW,
        WindowProc,
        0,
        sizeof(void *),
        hInstance,
        LoadIconW(NULL, IDI_APPLICATION),
        LoadCursorW(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        NULL,
        L"NvGL4",
        NULL};
    ATOM hWndClass = RegisterClassExW(&windowClass);

    UINT windowExStyle = WS_EX_APPWINDOW;
    UINT windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE | CS_OWNDC | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    RECT rect = {
        0,
        0,
        params.backBufferWidth,
        params.backBufferHeight};
    AdjustWindowRectEx(&rect, windowStyle, FALSE, windowExStyle);

    m_hWnd = CreateWindowExW(
        windowExStyle,
        MAKEINTATOM(hWndClass),
        title,
        windowStyle,
        rect.left + 100,
        rect.top + 100,
        rect.right - rect.left,
        rect.bottom - rect.top,
        GetDesktopWindow(),
        NULL,
        hInstance,
        NULL);

    if (!m_hWnd)
    {
#ifdef DEBUG
        DWORD errorCode = GetLastError();
        printf("CreateWindowEx error code = 0x%x\n", errorCode);
#endif

        MessageBox(NULL, L"Cannot create window", m_WindowTitle.c_str(), MB_OK | MB_ICONERROR);
        return E_FAIL;
    }

    SetWindowLongPtr(m_hWnd, 0, (LONG_PTR)this);
    UpdateWindow(m_hWnd);

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    UINT width = clientRect.right - clientRect.left;
    UINT height = clientRect.bottom - clientRect.top;
    m_WindowSize = {LONG(width), LONG(height)};

    m_DC = GetDC(m_hWnd);

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), // size of this pfd
        1,                             // version number
        PFD_DRAW_TO_WINDOW |           // support window
            PFD_SUPPORT_OPENGL |       // support OpenGL
            PFD_DOUBLEBUFFER,          // double buffered
        PFD_TYPE_RGBA,                 // RGBA type
        32,                            // 32-bit color depth
        0, 0, 0, 0, 0, 0,              // color bits ignored
        0,                             // no alpha buffer
        0,                             // shift bit ignored
        0,                             // no accumulation buffer
        0, 0, 0, 0,                    // accum bits ignored
        0,                             // no z-buffer
        0,                             // no stencil buffer
        0,                             // no auxiliary buffer
        PFD_MAIN_PLANE,                // main layer
        0,                             // reserved
        0, 0, 0                        // layer masks ignored
    };

    // get the best available match of pixel format for the device context
    int iPixelFormat = ChoosePixelFormat(m_DC, &pfd);

    if (iPixelFormat == 0)
    {
        MessageBox(NULL, L"Failed to choose pixel format", m_WindowTitle.c_str(), MB_OK | MB_ICONERROR);
        return E_FAIL;
    }

    // make that the pixel format of the device context
    SetPixelFormat(m_DC, iPixelFormat, &pfd);

    DescribePixelFormat(m_DC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    HGLRC tmp_gl_context = wglCreateContext(m_DC);

    if (0 == tmp_gl_context)
    {
        MessageBox(NULL, L"Failed to create a GL context", m_WindowTitle.c_str(), MB_OK | MB_ICONERROR);
        return E_FAIL;
    }

    wglMakeCurrent(m_DC, tmp_gl_context);

    if (0 == gladLoadWGLLoader(GladLoadProc, m_DC))
    {
        MessageBox(NULL, L"Failed to initialize Glad WGL library", m_WindowTitle.c_str(), MB_OK | MB_ICONERROR);
        return E_FAIL;
    }

#if 1
    int attribList[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 5,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        WGL_CONTEXT_FLAGS_ARB, params.enableDebugRuntime ? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
        0};

    m_GLcontext = wglCreateContextAttribsARB(m_DC, NULL, attribList);

    if (0 == m_GLcontext)
    {
        MessageBox(NULL, L"Failed to create a GL context", m_WindowTitle.c_str(), MB_OK | MB_ICONERROR);
        return E_FAIL;
    }

    wglMakeCurrent(m_DC, 0);

    wglDeleteContext(tmp_gl_context);

    wglMakeCurrent(m_DC, m_GLcontext);
#endif

    if (0 == gladLoadGLLoader(GladLoadProc))
    {
        MessageBox(NULL, L"Failed to initialize Glad GL library", m_WindowTitle.c_str(), MB_OK | MB_ICONERROR);
        return E_FAIL;
    }

    const char *glVersion = (const char *)glGetString(GL_VERSION);
    OutputDebugStringA("OpenGL version: ");
    OutputDebugStringA(glVersion);
    OutputDebugStringA("\n");

    if (params.enableDebugRuntime)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugMessageProc, nullptr);
    }

    DeviceCreated();
    BackBufferResized();

    return S_OK;
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DeviceManager *pDM = (DeviceManager *)GetWindowLongPtr(hWnd, 0);
    if (pDM)
        return pDM->MsgProc(hWnd, uMsg, wParam, lParam);
    else
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void *GladLoadProc(const char *name)
{
    void *wgl_proc = wglGetProcAddress(name);
    if (NULL == wgl_proc)
    {
        wgl_proc = GetProcAddress(hInstance_OpenGL32, name);
    }
    return wgl_proc;
}

static void GLAPIENTRY GLDebugMessageProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    (void)id;
    (void)length;
    (void)userParam;

    if (type == GL_DEBUG_TYPE_OTHER && severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;

    const char *sourceStr = "OTHER";
    switch (source)
    {
    case GL_DEBUG_SOURCE_API:
        sourceStr = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        sourceStr = "WINDOW_SYSTEM";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        sourceStr = "SHADER_COMPILER";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        sourceStr = "THIRD_PARTY";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        sourceStr = "APPLICATION";
        break;
    }

    const char *typeStr = "OTHER";
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        typeStr = "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeStr = "DEPRECATED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeStr = "UNDEFINED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        typeStr = "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        typeStr = "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_MARKER:
        typeStr = "MARKER";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        typeStr = "PUSH_GROUP";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        typeStr = "POP_GROUP";
        break;
    }

    const char *severityStr = "UNKNOWN";
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        severityStr = "HIGH";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        severityStr = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        severityStr = "LOW";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        severityStr = "NOTIFICATION";
        break;
    }

    OutputDebugStringA("OpenGL Message [");
    OutputDebugStringA(typeStr);
    OutputDebugStringA(", ");
    OutputDebugStringA(severityStr);
    OutputDebugStringA("] from [");
    OutputDebugStringA(sourceStr);
    OutputDebugStringA("]: ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

void DeviceManager::Shutdown()
{
    if (m_ShutdownCalled)
        return;

    m_ShutdownCalled = true;

    DeviceDestroyed();

    wglMakeCurrent(0, 0);
    wglDeleteContext(m_GLcontext);

    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
    }
}

void DeviceManager::MessageLoop()
{
    MSG msg = {0};

    LARGE_INTEGER perfFreq, previousTime;
    QueryPerformanceFrequency(&perfFreq);
    QueryPerformanceCounter(&previousTime);

    while (WM_QUIT != msg.message)
    {
#if ENABLE_XINPUT
        XINPUT_KEYSTROKE xInputKeystroke;
        memset(&xInputKeystroke, 0, sizeof(xInputKeystroke));
        const int gamepadIndex = 0;

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else if (XInputGetKeystroke(gamepadIndex, 0, &xInputKeystroke) == ERROR_SUCCESS && xInputKeystroke.Flags != 0)
        {
            if (xInputKeystroke.Flags & (XINPUT_KEYSTROKE_KEYDOWN | XINPUT_KEYSTROKE_REPEAT))
            {
                MsgProc(m_hWnd, WM_KEYDOWN, xInputKeystroke.VirtualKey, 0);
            }

            if (xInputKeystroke.Flags & XINPUT_KEYSTROKE_KEYUP)
            {
                MsgProc(m_hWnd, WM_KEYUP, xInputKeystroke.VirtualKey, 0);
            }
        }
#else
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#endif
        else
        {
            LARGE_INTEGER newTime;
            QueryPerformanceCounter(&newTime);

            double elapsedSeconds = (m_FixedFrameInterval >= 0)
                                        ? m_FixedFrameInterval
                                        : (double)(newTime.QuadPart - previousTime.QuadPart) / (double)perfFreq.QuadPart;

            if (m_GLcontext && GetWindowState() != kWindowMinimized)
            {
                Animate(elapsedSeconds);
                Render();
                wglSwapIntervalEXT(m_SyncInterval);
                SwapBuffers(m_DC);
                Sleep(0);
            }
            else
            {
                // Release CPU resources when idle
                Sleep(1);
            }

            {
                m_vFrameTimes.push_back(elapsedSeconds);
                double timeSum = 0;
                for (auto it = m_vFrameTimes.begin(); it != m_vFrameTimes.end(); it++)
                    timeSum += *it;

                if (timeSum > m_AverageTimeUpdateInterval)
                {
                    m_AverageFrameTime = timeSum / (double)m_vFrameTimes.size();
                    m_vFrameTimes.clear();
                }
            }

            previousTime = newTime;
        }
    }
}

LRESULT DeviceManager::MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_SYSKEYDOWN:
        if (wParam == VK_F4)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_ENTERSIZEMOVE:
        m_InSizingModalLoop = true;
        break;

    case WM_EXITSIZEMOVE:
        m_InSizingModalLoop = false;
        BackBufferResized();
        break;

    case WM_SIZE:
        // Ignore the WM_SIZE event if there is no device,
        // or if the window has been minimized (size == 0),
        // or if it has been restored to the previous size (this part is tested inside ResizeSwapChain)
        if (m_GLcontext && (lParam != 0))
        {
            m_WindowSize.cx = LOWORD(lParam);
            m_WindowSize.cy = HIWORD(lParam);

            if (!m_InSizingModalLoop)
                BackBufferResized();
        }
    }

    if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST ||
        uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
    {
        // processing messages front-to-back
        for (auto it = m_vControllers.begin(); it != m_vControllers.end(); it++)
        {
            if ((*it)->IsEnabled())
            {
                // for kb/mouse messages, 0 means the message has been handled
                if (0 == (*it)->MsgProc(hWnd, uMsg, wParam, lParam))
                    return 0;
            }
        }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void DeviceManager::Render()
{
    if (m_EnableRenderTargetClear)
    {
        glClearColor(m_RenderTargetClearColor[0], m_RenderTargetClearColor[1], m_RenderTargetClearColor[2], m_RenderTargetClearColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // rendering back-to-front
    for (auto it = m_vControllers.rbegin(); it != m_vControllers.rend(); it++)
    {
        if ((*it)->IsEnabled())
        {
            glViewport(0, 0, m_WindowSize.cx, m_WindowSize.cy);

            (*it)->Render(0);
        }
    }
}

void DeviceManager::Animate(double fElapsedTimeSeconds)
{
    // front-to-back, but the order shouldn't matter
    for (auto it = m_vControllers.begin(); it != m_vControllers.end(); it++)
    {
        if ((*it)->IsEnabled())
        {
            (*it)->Animate(fElapsedTimeSeconds);
        }
    }
}

void DeviceManager::DeviceCreated()
{
    // creating resources front-to-back
    for (auto it = m_vControllers.begin(); it != m_vControllers.end(); it++)
    {
        (*it)->DeviceCreated();
    }
}

void DeviceManager::DeviceDestroyed()
{
    // releasing resources back-to-front
    for (auto it = m_vControllers.rbegin(); it != m_vControllers.rend(); it++)
    {
        (*it)->DeviceDestroyed();
    }
}

void DeviceManager::BackBufferResized()
{
    if (m_GLcontext == 0)
        return;

    for (auto it = m_vControllers.begin(); it != m_vControllers.end(); it++)
    {
        (*it)->BackBufferResized(m_WindowSize.cx, m_WindowSize.cy, 1);
    }
}

void DeviceManager::AddControllerToFront(IVisualController *pController)
{
    m_vControllers.remove(pController);
    m_vControllers.push_front(pController);
}

void DeviceManager::AddControllerToBack(IVisualController *pController)
{
    m_vControllers.remove(pController);
    m_vControllers.push_back(pController);
}

void DeviceManager::RemoveController(IVisualController *pController)
{
    m_vControllers.remove(pController);
}

HRESULT DeviceManager::ResizeWindow(int width, int height)
{
    if (m_GLcontext == 0)
        return E_FAIL;

    RECT rect;
    GetWindowRect(m_hWnd, &rect);

    ShowWindow(m_hWnd, SW_RESTORE);

    if (!MoveWindow(m_hWnd, rect.left, rect.top, width, height, true))
        return E_FAIL;

    // No need to call m_SwapChain->ResizeBackBuffer because MoveWindow will send WM_SIZE, which calls that function.

    return S_OK;
}

DeviceManager::WindowState DeviceManager::GetWindowState()
{
    if (m_hWnd == INVALID_HANDLE_VALUE)
        return kWindowNone;

    if (IsZoomed(m_hWnd))
        return kWindowMaximized;

    if (IsIconic(m_hWnd))
        return kWindowMinimized;

    return kWindowNormal;
}

HRESULT DeviceManager::GetDisplayResolution(int &width, int &height)
{
    if (m_hWnd != INVALID_HANDLE_VALUE)
    {
        HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info;
        info.cbSize = sizeof(MONITORINFO);

        if (GetMonitorInfo(monitor, &info))
        {
            width = info.rcMonitor.right - info.rcMonitor.left;
            height = info.rcMonitor.bottom - info.rcMonitor.top;
            return S_OK;
        }
    }

    return E_FAIL;
}

void DeviceManager::SetPrimaryRenderTargetClearColor(bool enableClear, const float *pColor)
{
    m_EnableRenderTargetClear = enableClear;

    if (pColor)
        memcpy(m_RenderTargetClearColor, pColor, sizeof(float) * 4);
}
