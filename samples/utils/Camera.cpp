/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

//--------------------------------------------------------------------------------------
// File: DXUTcamera.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------

#include "Camera.h"

using namespace DirectX;

#ifdef WITH_XINPUT
#include <XInput.h>
#endif

//--------------------------------------------------------------------------------------
CD3DArcBall::CD3DArcBall()
{
    Reset();
    m_vDownPt = XMVectorReplicate(0);
    m_vCurrentPt = XMVectorReplicate(0);
    m_Offset.x = m_Offset.y = 0;

    RECT rc;
    GetClientRect(GetForegroundWindow(), &rc);
    SetWindow(rc.right, rc.bottom);
}

//--------------------------------------------------------------------------------------
void CD3DArcBall::Reset()
{
    m_qDown = XMQuaternionIdentity();
    m_qNow = XMQuaternionIdentity();
    m_mTranslation = XMMatrixIdentity();
    m_mTranslationDelta = XMMatrixIdentity();
    m_bDrag = FALSE;
    m_fRadiusTranslation = 1.0f;
    m_fRadius = 1.0f;
}

//--------------------------------------------------------------------------------------
XMVECTOR CD3DArcBall::ScreenToVector(float fScreenPtX, float fScreenPtY)
{
    // Scale to screen
    FLOAT x = -(fScreenPtX - m_Offset.x - m_nWidth / 2) / (m_fRadius * m_nWidth / 2);
    FLOAT y = (fScreenPtY - m_Offset.y - m_nHeight / 2) / (m_fRadius * m_nHeight / 2);

    FLOAT z = 0.0f;
    FLOAT mag = x * x + y * y;

    if (mag > 1.0f)
    {
        FLOAT scale = 1.0f / sqrtf(mag);
        x *= scale;
        y *= scale;
    }
    else
        z = sqrtf(1.0f - mag);

    // Return vector
    return XMVectorSet(x, y, z, 0);
}

//--------------------------------------------------------------------------------------
XMVECTOR CD3DArcBall::QuatFromBallPoints(XMVECTOR vFrom, XMVECTOR vTo)
{
    float fDot = XMVectorGetX(XMVector3Dot(vFrom, vTo));
    XMVECTOR vPart = XMVector3Cross(vFrom, vTo);

    return XMVectorSetW(vPart, fDot);
}

//--------------------------------------------------------------------------------------
void CD3DArcBall::OnBegin(int nX, int nY)
{
    // Only enter the drag state if the click falls
    // inside the click rectangle.
    if (nX >= m_Offset.x &&
        nX < m_Offset.x + m_nWidth &&
        nY >= m_Offset.y &&
        nY < m_Offset.y + m_nHeight)
    {
        m_bDrag = true;
        m_qDown = m_qNow;
        m_vDownPt = ScreenToVector((float)nX, (float)nY);
    }
}

//--------------------------------------------------------------------------------------
void CD3DArcBall::OnMove(int nX, int nY)
{
    if (m_bDrag)
    {
        m_vCurrentPt = ScreenToVector((float)nX, (float)nY);
        m_qNow = XMQuaternionMultiply(m_qDown, QuatFromBallPoints(m_vDownPt, m_vCurrentPt));
    }
}

//--------------------------------------------------------------------------------------
void CD3DArcBall::OnEnd()
{
    m_bDrag = false;
}

//--------------------------------------------------------------------------------------
// Desc:
//--------------------------------------------------------------------------------------
LRESULT CD3DArcBall::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Current mouse position
    int iMouseX = (short)LOWORD(lParam);
    int iMouseY = (short)HIWORD(lParam);

    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(hWnd);
        OnBegin(iMouseX, iMouseY);
        return TRUE;

    case WM_LBUTTONUP:
        ReleaseCapture();
        OnEnd();
        return TRUE;
    case WM_CAPTURECHANGED:
        if ((HWND)lParam != hWnd)
        {
            ReleaseCapture();
            OnEnd();
        }
        return TRUE;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        SetCapture(hWnd);
        // Store off the position of the cursor when the button is pressed
        m_ptLastMouse.x = iMouseX;
        m_ptLastMouse.y = iMouseY;
        return TRUE;

    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        ReleaseCapture();
        return TRUE;

    case WM_MOUSEMOVE:
        if (MK_LBUTTON & wParam)
        {
            OnMove(iMouseX, iMouseY);
        }
        else if ((MK_RBUTTON & wParam) || (MK_MBUTTON & wParam))
        {
            // Normalize based on size of window and bounding sphere radius
            FLOAT fDeltaX = (m_ptLastMouse.x - iMouseX) * m_fRadiusTranslation / m_nWidth;
            FLOAT fDeltaY = (m_ptLastMouse.y - iMouseY) * m_fRadiusTranslation / m_nHeight;

            if (wParam & MK_RBUTTON)
            {
                m_mTranslationDelta = XMMatrixTranslation(-2 * fDeltaX, 2 * fDeltaY, 0.0f);
                m_mTranslation *= m_mTranslationDelta;
            }
            else // wParam & MK_MBUTTON
            {
                m_mTranslationDelta = XMMatrixTranslation(0.0f, 0.0f, 5 * fDeltaY);
                m_mTranslation *= m_mTranslationDelta;
            }

            // Store mouse coordinate
            m_ptLastMouse.x = iMouseX;
            m_ptLastMouse.y = iMouseY;
        }
        return TRUE;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
CBaseCamera::CBaseCamera()
{
    m_cKeysDown = 0;
    ZeroMemory(m_aKeys, sizeof(BYTE) * CAM_MAX_KEYS);

    // Set attributes for the view matrix
    XMVECTOR vEyePt = XMVectorReplicate(0);
    XMVECTOR vLookatPt = XMVectorSet(0, 0, 1, 0);
    XMVECTOR vUp = XMVectorSet(0, 1, 0, 0);

    // Setup the view matrix
    SetViewParams(vEyePt, vLookatPt, vUp);

    // Setup the projection matrix
    SetProjParams(XM_PIDIV4, 1.0f, 1.0f, 1000.0f);

    GetCursorPos(&m_ptLastMousePosition);
    m_bMouseLButtonDown = false;
    m_bMouseMButtonDown = false;
    m_bMouseRButtonDown = false;
    m_nCurrentButtonMask = 0;
    m_nMouseWheelDelta = 0;

    m_fCameraYawAngle = 0.0f;
    m_fCameraPitchAngle = 0.0f;

    SetRect(&m_rcDrag, LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX);
    m_vVelocity = XMVectorReplicate(0);
    m_bMovementDrag = false;
    m_vVelocityDrag = XMVectorReplicate(0);
    m_fDragTimer = 0.0f;
    m_fTotalDragTimeToZero = 0.25;
    m_vRotVelocity = XMVectorReplicate(0);

    m_fRotationScaler = 0.01f;
    m_fMoveScaler = 5.0f;

    m_bInvertPitch = false;
    m_bEnableYAxisMovement = true;
    m_bEnablePositionMovement = true;

    m_vMouseDelta = XMVectorReplicate(0);
    m_fFramesToSmoothMouseData = 2.0f;

    m_bClipToBoundary = false;
    m_vMinBoundary = XMVectorReplicate(-1);
    m_vMaxBoundary = XMVectorReplicate(1);

    m_bResetCursorAfterMove = false;
}

//--------------------------------------------------------------------------------------
// Client can call this to change the position and direction of camera
//--------------------------------------------------------------------------------------
VOID CBaseCamera::SetViewParams(XMVECTOR vEyePt, XMVECTOR vLookatPt, XMVECTOR vUp)
{
    m_vDefaultEye = m_vEye = vEyePt;
    m_vDefaultLookAt = m_vLookAt = vLookatPt;
    m_vDefaultUp = m_vUp = vUp;

    // Calc the view matrix
    m_mView = XMMatrixLookAtLH(vEyePt, vLookatPt, vUp);

    XMVECTOR det;
    XMMATRIX mInvView = XMMatrixInverse(&det, m_mView);

    // The axis basis vectors and camera position are stored inside the
    // position matrix in the 4 rows of the camera's world matrix.
    // To figure out the yaw/pitch of the camera, we just need the Z basis vector
    XMVECTOR vZBasis = mInvView.r[2];

    m_fCameraYawAngle = atan2f(XMVectorGetX(vZBasis), XMVectorGetZ(vZBasis));
    float fLen = sqrtf(XMVectorGetX(vZBasis) * XMVectorGetX(vZBasis) + XMVectorGetZ(vZBasis) * XMVectorGetZ(vZBasis));
    m_fCameraPitchAngle = -atan2f(XMVectorGetY(vZBasis), fLen);
}

//--------------------------------------------------------------------------------------
// Calculates the projection matrix based on input params
//--------------------------------------------------------------------------------------
VOID CBaseCamera::SetProjParams(FLOAT fFOV, FLOAT fAspect, FLOAT fNearPlane,
                                FLOAT fFarPlane)
{
    // Set attributes for the projection matrix
    m_fFOV = fFOV;
    m_fAspect = fAspect;
    m_fNearPlane = fNearPlane;
    m_fFarPlane = fFarPlane;

    m_mProj = XMMatrixPerspectiveFovLH(fFOV, fAspect, fNearPlane, fFarPlane);
}

//--------------------------------------------------------------------------------------
// Call this from your message proc so this class can handle window messages
//--------------------------------------------------------------------------------------
LRESULT CBaseCamera::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case WM_KEYDOWN:
    {
        // Map this key to a D3DUtil_CameraKeys enum and update the
        // state of m_aKeys[] by adding the KEY_WAS_DOWN_MASK|KEY_IS_DOWN_MASK mask
        // only if the key is not down
        D3DUtil_CameraKeys mappedKey = MapKey((UINT)wParam);
        if (mappedKey != CAM_UNKNOWN)
        {
            if (FALSE == IsKeyDown(m_aKeys[mappedKey]))
            {
                m_aKeys[mappedKey] = KEY_WAS_DOWN_MASK | KEY_IS_DOWN_MASK;
                ++m_cKeysDown;
            }
        }
        break;
    }

    case WM_KEYUP:
    {
        // Map this key to a D3DUtil_CameraKeys enum and update the
        // state of m_aKeys[] by removing the KEY_IS_DOWN_MASK mask.
        D3DUtil_CameraKeys mappedKey = MapKey((UINT)wParam);
        if (mappedKey != CAM_UNKNOWN && (DWORD)mappedKey < 8)
        {
            m_aKeys[mappedKey] &= ~KEY_IS_DOWN_MASK;
            --m_cKeysDown;
        }
        break;
    }

    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_LBUTTONDBLCLK:
    {
        // Compute the drag rectangle in screen coord.
        POINT ptCursor =
            {
                (short)LOWORD(lParam), (short)HIWORD(lParam)};

        // Update member var state
        if ((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && PtInRect(&m_rcDrag, ptCursor))
        {
            m_bMouseLButtonDown = true;
            m_nCurrentButtonMask |= MOUSE_LEFT_BUTTON;
        }
        if ((uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK) && PtInRect(&m_rcDrag, ptCursor))
        {
            m_bMouseMButtonDown = true;
            m_nCurrentButtonMask |= MOUSE_MIDDLE_BUTTON;
        }
        if ((uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK) && PtInRect(&m_rcDrag, ptCursor))
        {
            m_bMouseRButtonDown = true;
            m_nCurrentButtonMask |= MOUSE_RIGHT_BUTTON;
        }

        // Capture the mouse, so if the mouse button is
        // released outside the window, we'll get the WM_LBUTTONUP message
        SetCapture(hWnd);
        GetCursorPos(&m_ptLastMousePosition);
        return TRUE;
    }

    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_LBUTTONUP:
    {
        // Update member var state
        if (uMsg == WM_LBUTTONUP)
        {
            m_bMouseLButtonDown = false;
            m_nCurrentButtonMask &= ~MOUSE_LEFT_BUTTON;
        }
        if (uMsg == WM_MBUTTONUP)
        {
            m_bMouseMButtonDown = false;
            m_nCurrentButtonMask &= ~MOUSE_MIDDLE_BUTTON;
        }
        if (uMsg == WM_RBUTTONUP)
        {
            m_bMouseRButtonDown = false;
            m_nCurrentButtonMask &= ~MOUSE_RIGHT_BUTTON;
        }

        // Release the capture if no mouse buttons down
        if (!m_bMouseLButtonDown &&
            !m_bMouseRButtonDown &&
            !m_bMouseMButtonDown)
        {
            ReleaseCapture();
        }
        break;
    }

    case WM_CAPTURECHANGED:
    {
        if ((HWND)lParam != hWnd)
        {
            if ((m_nCurrentButtonMask & MOUSE_LEFT_BUTTON) ||
                (m_nCurrentButtonMask & MOUSE_MIDDLE_BUTTON) ||
                (m_nCurrentButtonMask & MOUSE_RIGHT_BUTTON))
            {
                m_bMouseLButtonDown = false;
                m_bMouseMButtonDown = false;
                m_bMouseRButtonDown = false;
                m_nCurrentButtonMask &= ~MOUSE_LEFT_BUTTON;
                m_nCurrentButtonMask &= ~MOUSE_MIDDLE_BUTTON;
                m_nCurrentButtonMask &= ~MOUSE_RIGHT_BUTTON;
                ReleaseCapture();
            }
        }
        break;
    }

    case WM_MOUSEWHEEL:
        // Update member var state
        m_nMouseWheelDelta += (short)HIWORD(wParam);
        break;
    }

    return FALSE;
}

void TransformGamepadStick(float &value)
{
    if (fabs(value) < 0.03f)
        value = 0;
    else
        value = value * value * (value < 0 ? -1 : 1);
}

//--------------------------------------------------------------------------------------
// Figure out the velocity based on keyboard input & drag if any
//--------------------------------------------------------------------------------------
void CBaseCamera::GetInput(bool bGetKeyboardInput, bool bGetMouseInput, bool bGetGamepadInput,
                           bool bResetCursorAfterMove)
{
    (void)bResetCursorAfterMove;

    m_vKeyboardDirection = XMVectorReplicate(0);
    if (bGetKeyboardInput)
    {
        float kx = 0, ky = 0, kz = 0;

        // Update acceleration vector based on keyboard state
        if (IsKeyDown(m_aKeys[CAM_MOVE_FORWARD]))
            kz += 1.0f;
        if (IsKeyDown(m_aKeys[CAM_MOVE_BACKWARD]))
            kz -= 1.0f;
        if (m_bEnableYAxisMovement)
        {
            if (IsKeyDown(m_aKeys[CAM_MOVE_UP]))
                ky += 1.0f;
            if (IsKeyDown(m_aKeys[CAM_MOVE_DOWN]))
                ky -= 1.0f;
        }
        if (IsKeyDown(m_aKeys[CAM_STRAFE_RIGHT]))
            kx += 1.0f;
        if (IsKeyDown(m_aKeys[CAM_STRAFE_LEFT]))
            kx -= 1.0f;

        m_vKeyboardDirection = XMVectorSet(kx, ky, kz, 0);

        if (GetKeyState(VK_SHIFT) & 0x8000)
            m_vKeyboardDirection *= 10.0f;
        else if (GetKeyState(VK_CONTROL) & 0x8000)
            m_vKeyboardDirection *= 0.1f;
    }

    if (bGetMouseInput)
    {
        UpdateMouseDelta();
    }

#ifdef WITH_XINPUT
    if (bGetGamepadInput)
    {
        m_vGamePadLeftThumb = XMVectorReplicate(0);
        m_vGamePadRightThumb = XMVectorReplicate(0);

        XINPUT_STATE gamepadState;
        const int gamepadIndex = 0;

        if (ERROR_SUCCESS == XInputGetState(gamepadIndex, &gamepadState))
        {
            const float maxStick = 32768.0f;

            float fThumbLX = (float)gamepadState.Gamepad.sThumbLX / maxStick;
            float fThumbLY = (float)gamepadState.Gamepad.sThumbLY / maxStick;
            float fThumbRX = (float)gamepadState.Gamepad.sThumbRX / maxStick;
            float fThumbRY = (float)gamepadState.Gamepad.sThumbRY / maxStick;

            TransformGamepadStick(fThumbLX);
            TransformGamepadStick(fThumbLY);
            TransformGamepadStick(fThumbRX);
            TransformGamepadStick(fThumbRY);

            if (gamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_X)
            {
                m_vGamePadLeftThumb = XMVectorSet(fThumbLX, fThumbLY, 0, 0);
            }
            else
            {
                m_vGamePadLeftThumb = XMVectorSet(fThumbLX, 0, fThumbLY, 0);
            }

            m_vGamePadRightThumb = XMVectorSet(fThumbRX, 0, fThumbRY, 0);
        }
    }
#else
    (void)bGetGamepadInput;
#endif
}

//--------------------------------------------------------------------------------------
// Figure out the mouse delta based on mouse movement
//--------------------------------------------------------------------------------------
void CBaseCamera::UpdateMouseDelta()
{
    POINT ptCurMousePos;

    // Get current position of mouse
    GetCursorPos(&ptCurMousePos);

    // Calc how far it's moved since last frame
    XMVECTOR ptCurMouseDelta = XMVectorSet(
        (float)(ptCurMousePos.x - m_ptLastMousePosition.x),
        (float)(ptCurMousePos.y - m_ptLastMousePosition.y),
        0, 0);

    // Record current position for next time
    m_ptLastMousePosition = ptCurMousePos;

    // Smooth the relative mouse data over a few frames so it isn't
    // jerky when moving slowly at low frame rates.
    float fPercentOfNew = 1.0f / m_fFramesToSmoothMouseData;
    float fPercentOfOld = 1.0f - fPercentOfNew;

    m_vMouseDelta = m_vMouseDelta * fPercentOfOld + ptCurMouseDelta * fPercentOfNew;

    m_vRotVelocity = m_vMouseDelta * m_fRotationScaler;
}

//--------------------------------------------------------------------------------------
// Figure out the velocity based on keyboard input & drag if any
//--------------------------------------------------------------------------------------
void CBaseCamera::UpdateVelocity(float fElapsedTime)
{
    XMVECTOR vGamePadRightThumb = XMVectorSet(XMVectorGetX(m_vGamePadRightThumb), -XMVectorGetZ(m_vGamePadRightThumb), 0, 0);
    m_vRotVelocity = m_vMouseDelta * m_fRotationScaler + vGamePadRightThumb * 0.02f;

    XMVECTOR vAccel = m_vKeyboardDirection + m_vGamePadLeftThumb;

    // Scale the acceleration vector
    vAccel *= m_fMoveScaler;

    if (m_bMovementDrag)
    {
        // Is there any acceleration this frame?
        if (XMVectorGetX(XMVector3Length(vAccel)) > 0)
        {
            // If so, then this means the user has pressed a movement key\
            // so change the velocity immediately to acceleration
            // upon keyboard input.  This isn't normal physics
            // but it will give a quick response to keyboard input
            m_vVelocity = vAccel;
            m_fDragTimer = m_fTotalDragTimeToZero;
            m_vVelocityDrag = vAccel / m_fDragTimer;
        }
        else
        {
            // If no key being pressed, then slowly decrease velocity to 0
            if (m_fDragTimer > 0)
            {
                // Drag until timer is <= 0
                m_vVelocity -= m_vVelocityDrag * fElapsedTime;
                m_fDragTimer -= fElapsedTime;
            }
            else
            {
                // Zero velocity
                m_vVelocity = XMVectorReplicate(0);
            }
        }
    }
    else
    {
        // No drag, so immediately change the velocity
        m_vVelocity = vAccel;
    }
}

//--------------------------------------------------------------------------------------
// Clamps pV to lie inside m_vMinBoundary & m_vMaxBoundary
//--------------------------------------------------------------------------------------
void CBaseCamera::ConstrainToBoundary(XMVECTOR &pV)
{
    // Constrain vector to a bounding box
    pV = XMVectorMin(m_vMaxBoundary, XMVectorMax(m_vMinBoundary, pV));
}

//--------------------------------------------------------------------------------------
// Maps a windows virtual key to an enum
//--------------------------------------------------------------------------------------
D3DUtil_CameraKeys CBaseCamera::MapKey(UINT nKey)
{
    // This could be upgraded to a method that's user-definable but for
    // simplicity, we'll use a hardcoded mapping.
    switch (nKey)
    {
    case VK_CONTROL:
        return CAM_CONTROLDOWN;
    case VK_LEFT:
        return CAM_STRAFE_LEFT;
    case VK_RIGHT:
        return CAM_STRAFE_RIGHT;
    case VK_UP:
        return CAM_MOVE_FORWARD;
    case VK_DOWN:
        return CAM_MOVE_BACKWARD;
    case VK_PRIOR:
        return CAM_MOVE_UP; // pgup
    case VK_NEXT:
        return CAM_MOVE_DOWN; // pgdn

    case 'A':
        return CAM_STRAFE_LEFT;
    case 'D':
        return CAM_STRAFE_RIGHT;
    case 'W':
        return CAM_MOVE_FORWARD;
    case 'S':
        return CAM_MOVE_BACKWARD;
    case 'Q':
        return CAM_MOVE_DOWN;
    case 'E':
        return CAM_MOVE_UP;

    case VK_NUMPAD4:
        return CAM_STRAFE_LEFT;
    case VK_NUMPAD6:
        return CAM_STRAFE_RIGHT;
    case VK_NUMPAD8:
        return CAM_MOVE_FORWARD;
    case VK_NUMPAD2:
        return CAM_MOVE_BACKWARD;
    case VK_NUMPAD9:
        return CAM_MOVE_UP;
    case VK_NUMPAD3:
        return CAM_MOVE_DOWN;

    case VK_HOME:
        return CAM_RESET;
    }

    return CAM_UNKNOWN;
}

//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID CBaseCamera::Reset()
{
    SetViewParams(m_vDefaultEye, m_vDefaultLookAt, m_vUp);
}

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
CFirstPersonCamera::CFirstPersonCamera() : m_nActiveButtonMask(0x07)
{
    m_bRotateWithoutButtonDown = false;
}

//--------------------------------------------------------------------------------------
// Update the view matrix based on user input & elapsed time
//--------------------------------------------------------------------------------------
VOID CFirstPersonCamera::FrameMove(FLOAT fElapsedTime)
{
    if (IsKeyDown(m_aKeys[CAM_RESET]))
        Reset();

    // Get keyboard/mouse/gamepad input
    GetInput(m_bEnablePositionMovement, (m_nActiveButtonMask & m_nCurrentButtonMask) || m_bRotateWithoutButtonDown,
             true, m_bResetCursorAfterMove);

    //// Get the mouse movement (if any) if the mouse button are down
    // if( (m_nActiveButtonMask & m_nCurrentButtonMask) || m_bRotateWithoutButtonDown )
    //     UpdateMouseDelta( fElapsedTime );

    // Get amount of velocity based on the keyboard input and drag (if any)
    UpdateVelocity(fElapsedTime);

    // Simple euler method to calculate position delta
    XMVECTOR vPosDelta = m_vVelocity * fElapsedTime;

    // If rotating the camera
    if ((m_nActiveButtonMask & m_nCurrentButtonMask) ||
        m_bRotateWithoutButtonDown ||
        XMVectorGetX(m_vGamePadRightThumb) != 0 ||
        XMVectorGetZ(m_vGamePadRightThumb) != 0)
    {
        // Update the pitch & yaw angle based on mouse movement
        float fYawDelta = XMVectorGetX(m_vRotVelocity);
        float fPitchDelta = XMVectorGetY(m_vRotVelocity);

        // Invert pitch if requested
        if (m_bInvertPitch)
            fPitchDelta = -fPitchDelta;

        m_fCameraPitchAngle += fPitchDelta;
        m_fCameraYawAngle += fYawDelta;

        // Limit pitch to straight up or straight down
        m_fCameraPitchAngle = __max(-XM_PI / 2.0f, m_fCameraPitchAngle);
        m_fCameraPitchAngle = __min(+XM_PI / 2.0f, m_fCameraPitchAngle);
    }

    // Make a rotation matrix based on the camera's yaw & pitch
    XMMATRIX mCameraRot = XMMatrixRotationRollPitchYaw(m_fCameraPitchAngle, m_fCameraYawAngle, 0);

    // Transform vectors based on camera's rotation matrix
    XMVECTOR vLocalUp = XMVectorSet(0, 1, 0, 0);
    XMVECTOR vLocalAhead = XMVectorSet(0, 0, 1, 0);
    XMVECTOR vWorldUp = XMVector3TransformCoord(vLocalUp, mCameraRot);
    XMVECTOR vWorldAhead = XMVector3TransformCoord(vLocalAhead, mCameraRot);

    // Transform the position delta by the camera's rotation
    XMVECTOR vPosDeltaWorld;
    if (!m_bEnableYAxisMovement)
    {
        // If restricting Y movement, do not include pitch
        // when transforming position delta vector.
        mCameraRot = XMMatrixRotationRollPitchYaw(0, m_fCameraYawAngle, 0);
    }
    vPosDeltaWorld = XMVector3TransformCoord(vPosDelta, mCameraRot);

    // Move the eye position
    m_vEye += vPosDeltaWorld;
    if (m_bClipToBoundary)
        ConstrainToBoundary(m_vEye);

    // Update the lookAt position based on the eye position
    m_vLookAt = m_vEye + vWorldAhead;

    // Update the view matrix
    m_mView = XMMatrixLookAtLH(m_vEye, m_vLookAt, vWorldUp);

    XMVECTOR det;
    m_mCameraWorld = XMMatrixInverse(&det, m_mView);
}

//--------------------------------------------------------------------------------------
// Enable or disable each of the mouse buttons for rotation drag.
//--------------------------------------------------------------------------------------
void CFirstPersonCamera::SetRotateButtons(bool bLeft, bool bMiddle, bool bRight, bool bRotateWithoutButtonDown)
{
    m_nActiveButtonMask = (bLeft ? MOUSE_LEFT_BUTTON : 0) |
                          (bMiddle ? MOUSE_MIDDLE_BUTTON : 0) |
                          (bRight ? MOUSE_RIGHT_BUTTON : 0);
    m_bRotateWithoutButtonDown = bRotateWithoutButtonDown;
}

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
CModelViewerCamera::CModelViewerCamera()
{
    m_mWorld = XMMatrixIdentity();
    m_mModelRot = XMMatrixIdentity();
    m_mModelLastRot = XMMatrixIdentity();
    m_mCameraRotLast = XMMatrixIdentity();
    m_vModelCenter = XMVectorReplicate(0);
    m_fRadius = 5.0f;
    m_fDefaultRadius = 5.0f;
    m_fMinRadius = 1.0f;
    m_fMaxRadius = FLT_MAX;
    m_bLimitPitch = false;
    m_bEnablePositionMovement = false;
    m_bAttachCameraToModel = false;

    m_nRotateModelButtonMask = MOUSE_LEFT_BUTTON;
    m_nZoomButtonMask = MOUSE_WHEEL;
    m_nRotateCameraButtonMask = MOUSE_RIGHT_BUTTON;
    m_bDragSinceLastUpdate = true;
}

//--------------------------------------------------------------------------------------
// Update the view matrix & the model's world matrix based
//       on user input & elapsed time
//--------------------------------------------------------------------------------------
VOID CModelViewerCamera::FrameMove(FLOAT fElapsedTime)
{
    if (IsKeyDown(m_aKeys[CAM_RESET]))
        Reset();

    // If no dragged has happend since last time FrameMove is called,
    // and no camera key is held down, then no need to handle again.
    if (!m_bDragSinceLastUpdate && 0 == m_cKeysDown)
        return;
    m_bDragSinceLastUpdate = false;

    //// If no mouse button is held down,
    //// Get the mouse movement (if any) if the mouse button are down
    // if( m_nCurrentButtonMask != 0 )
    //     UpdateMouseDelta( fElapsedTime );

    GetInput(m_bEnablePositionMovement, m_nCurrentButtonMask != 0, true, false);

    // Get amount of velocity based on the keyboard input and drag (if any)
    UpdateVelocity(fElapsedTime);

    // Simple euler method to calculate position delta
    XMVECTOR vPosDelta = m_vVelocity * fElapsedTime;

    // Change the radius from the camera to the model based on wheel scrolling
    if (m_nMouseWheelDelta && m_nZoomButtonMask == MOUSE_WHEEL)
        m_fRadius -= m_nMouseWheelDelta * m_fRadius * 0.1f / 120.0f;
    m_fRadius = __min(m_fMaxRadius, m_fRadius);
    m_fRadius = __max(m_fMinRadius, m_fRadius);
    m_nMouseWheelDelta = 0;

    // Get the inverse of the arcball's rotation matrix
    XMVECTOR det;
    XMMATRIX mCameraRot = XMMatrixInverse(&det, m_ViewArcBall.GetRotationMatrix());

    // Transform vectors based on camera's rotation matrix
    XMVECTOR vLocalUp = XMVectorSet(0, 1, 0, 0);
    XMVECTOR vLocalAhead = XMVectorSet(0, 0, 1, 0);
    XMVECTOR vWorldUp = XMVector3TransformCoord(vLocalUp, mCameraRot);
    XMVECTOR vWorldAhead = XMVector3TransformCoord(vLocalAhead, mCameraRot);

    // Transform the position delta by the camera's rotation
    XMVECTOR vPosDeltaWorld = XMVector3TransformCoord(vPosDelta, mCameraRot);

    // Move the lookAt position
    m_vLookAt += vPosDeltaWorld;
    if (m_bClipToBoundary)
        ConstrainToBoundary(m_vLookAt);

    // Update the eye point based on a radius away from the lookAt position
    m_vEye = m_vLookAt - vWorldAhead * m_fRadius;

    // Update the view matrix
    m_mView = XMMatrixLookAtLH(m_vEye, m_vLookAt, vWorldUp);

    XMMATRIX mInvView = XMMatrixInverse(&det, m_mView);
    mInvView.r[3] = XMVectorSet(0, 0, 0, XMVectorGetW(mInvView.r[3]));

    XMMATRIX mModelLastRotInv = XMMatrixInverse(&det, m_mModelLastRot);

    // Accumulate the delta of the arcball's rotation in view space.
    // Note that per-frame delta rotations could be problematic over long periods of time.
    XMMATRIX mModelRot = m_WorldArcBall.GetRotationMatrix() * m_mView * mModelLastRotInv * mInvView;

    if (m_ViewArcBall.IsBeingDragged() && m_bAttachCameraToModel && !IsKeyDown(m_aKeys[CAM_CONTROLDOWN]))
    {
        // Attach camera to model by inverse of the model rotation
        XMMATRIX mCameraLastRotInv = XMMatrixInverse(&det, m_mCameraRotLast);
        XMMATRIX mCameraRotDelta = mCameraLastRotInv * mCameraRot; // local to world matrix
        m_mModelRot *= mCameraRotDelta;
    }
    m_mCameraRotLast = mCameraRot;

    m_mModelLastRot = mModelRot;

    // Since we're accumulating delta rotations, we need to orthonormalize
    // the matrix to prevent eventual matrix skew
    XMVECTOR &XBasis = m_mModelRot.r[0];
    XMVECTOR &YBasis = m_mModelRot.r[1];
    XMVECTOR &ZBasis = m_mModelRot.r[2];

    XBasis = XMVector3Normalize(XBasis);
    YBasis = XMVector3Normalize(XMVector3Cross(ZBasis, XBasis));
    ZBasis = XMVector3Cross(XBasis, YBasis);

    // Translate the rotation matrix to the same position as the lookAt position
    m_mModelRot.r[3] = XMVectorSetW(m_vLookAt, XMVectorGetW(m_mModelRot.r[3]));

    // Translate world matrix so its at the center of the model
    XMMATRIX mTrans = XMMatrixTranslation(-XMVectorGetX(m_vModelCenter), -XMVectorGetY(m_vModelCenter), -XMVectorGetZ(m_vModelCenter));
    m_mWorld = mTrans * m_mModelRot;
}

void CModelViewerCamera::SetDragRect(RECT &rc)
{
    CBaseCamera::SetDragRect(rc);

    m_WorldArcBall.SetOffset(rc.left, rc.top);
    m_ViewArcBall.SetOffset(rc.left, rc.top);
    SetWindow(rc.right - rc.left, rc.bottom - rc.top);
}

//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID CModelViewerCamera::Reset()
{
    CBaseCamera::Reset();

    m_mWorld = XMMatrixIdentity();
    m_mModelRot = XMMatrixIdentity();
    m_mModelLastRot = XMMatrixIdentity();
    m_mCameraRotLast = XMMatrixIdentity();

    m_fRadius = m_fDefaultRadius;
    m_WorldArcBall.Reset();
    m_ViewArcBall.Reset();
}

//--------------------------------------------------------------------------------------
// Override for setting the view parameters
//--------------------------------------------------------------------------------------
void CModelViewerCamera::SetViewParams(XMVECTOR vEyePt, XMVECTOR vLookatPt, XMVECTOR vUp)
{
    CBaseCamera::SetViewParams(vEyePt, vLookatPt, vUp);

    // Propogate changes to the member arcball
    XMMATRIX mRotation = XMMatrixLookAtLH(vEyePt, vLookatPt, vUp);
    XMVECTOR quat = XMQuaternionRotationMatrix(mRotation);
    m_ViewArcBall.SetQuatNow(quat);

    // Set the radius according to the distance
    XMVECTOR vEyeToPoint = vLookatPt - vEyePt;
    SetRadius(XMVectorGetX(XMVector3Length(vEyeToPoint)));

    // View information changed. FrameMove should be called.
    m_bDragSinceLastUpdate = true;
}

//--------------------------------------------------------------------------------------
// Call this from your message proc so this class can handle window messages
//--------------------------------------------------------------------------------------
LRESULT CModelViewerCamera::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CBaseCamera::HandleMessages(hWnd, uMsg, wParam, lParam);

    if (((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON) ||
        ((uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK) && m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON) ||
        ((uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK) && m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON))
    {
        int iMouseX = (short)LOWORD(lParam);
        int iMouseY = (short)HIWORD(lParam);
        m_WorldArcBall.OnBegin(iMouseX, iMouseY);
    }

    if (((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON) ||
        ((uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK) &&
         m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON) ||
        ((uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK) && m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON))
    {
        int iMouseX = (short)LOWORD(lParam);
        int iMouseY = (short)HIWORD(lParam);
        m_ViewArcBall.OnBegin(iMouseX, iMouseY);
    }

    if (uMsg == WM_MOUSEMOVE)
    {
        int iMouseX = (short)LOWORD(lParam);
        int iMouseY = (short)HIWORD(lParam);
        m_WorldArcBall.OnMove(iMouseX, iMouseY);
        m_ViewArcBall.OnMove(iMouseX, iMouseY);
    }

    if ((uMsg == WM_LBUTTONUP && m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON) ||
        (uMsg == WM_MBUTTONUP && m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON) ||
        (uMsg == WM_RBUTTONUP && m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON))
    {
        m_WorldArcBall.OnEnd();
    }

    if ((uMsg == WM_LBUTTONUP && m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON) ||
        (uMsg == WM_MBUTTONUP && m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON) ||
        (uMsg == WM_RBUTTONUP && m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON))
    {
        m_ViewArcBall.OnEnd();
    }

    if (uMsg == WM_CAPTURECHANGED)
    {
        if ((HWND)lParam != hWnd)
        {
            if ((m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON) ||
                (m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON) ||
                (m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON))
            {
                m_WorldArcBall.OnEnd();
            }

            if ((m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON) ||
                (m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON) ||
                (m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON))
            {
                m_ViewArcBall.OnEnd();
            }
        }
    }

    if (uMsg == WM_LBUTTONDOWN ||
        uMsg == WM_LBUTTONDBLCLK ||
        uMsg == WM_MBUTTONDOWN ||
        uMsg == WM_MBUTTONDBLCLK ||
        uMsg == WM_RBUTTONDOWN ||
        uMsg == WM_RBUTTONDBLCLK ||
        uMsg == WM_LBUTTONUP ||
        uMsg == WM_MBUTTONUP ||
        uMsg == WM_RBUTTONUP ||
        uMsg == WM_MOUSEWHEEL ||
        uMsg == WM_MOUSEMOVE)
    {
        m_bDragSinceLastUpdate = true;
    }

    return FALSE;
}
