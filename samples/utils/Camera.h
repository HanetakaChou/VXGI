/*
 * Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

//--------------------------------------------------------------------------------------
// File: DXUTcamera.h
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------

#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <float.h>

#pragma warning(disable : 4324) // structure was padded due to __declspec(align())

using DirectX::XMMATRIX;
using DirectX::XMVECTOR;

//--------------------------------------------------------------------------------------
class CD3DArcBall
{
public:
    CD3DArcBall();

    // Functions to change behavior
    void Reset();
    void SetTranslationRadius(float fRadiusTranslation)
    {
        m_fRadiusTranslation = fRadiusTranslation;
    }
    void SetWindow(int nWidth, int nHeight, float fRadius = 0.9f)
    {
        m_nWidth = nWidth;
        m_nHeight = nHeight;
        m_fRadius = fRadius;
        m_vCenter = DirectX::XMVectorSet(m_nWidth / 2.0f, m_nHeight / 2.0f, 0, 0);
    }
    void SetOffset(int nX, int nY)
    {
        m_Offset.x = nX;
        m_Offset.y = nY;
    }

    // Call these from client and use GetRotationMatrix() to read new rotation matrix
    void OnBegin(int nX, int nY); // start the rotation (pass current mouse position)
    void OnMove(int nX, int nY);  // continue the rotation (pass current mouse position)
    void OnEnd();                 // end the rotation

    // Or call this to automatically handle left, middle, right buttons
    LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Functions to get/set state
    XMMATRIX GetRotationMatrix()
    {
        return DirectX::XMMatrixRotationQuaternion(m_qNow);
    };
    XMMATRIX GetTranslationMatrix() const
    {
        return m_mTranslation;
    }
    XMMATRIX GetTranslationDeltaMatrix() const
    {
        return m_mTranslationDelta;
    }
    bool IsBeingDragged() const
    {
        return m_bDrag;
    }
    XMVECTOR GetQuatNow() const
    {
        return m_qNow;
    }
    void SetQuatNow(XMVECTOR q)
    {
        m_qNow = q;
    }

    static XMVECTOR WINAPI QuatFromBallPoints(XMVECTOR vFrom, XMVECTOR vTo);

protected:
    XMMATRIX m_mTranslation;      // Matrix for arc ball's position
    XMMATRIX m_mTranslationDelta; // Matrix for arc ball's position

    POINT m_Offset;             // window offset, or upper-left corner of window
    int m_nWidth;               // arc ball's window width
    int m_nHeight;              // arc ball's window height
    XMVECTOR m_vCenter;         // center of arc ball
    float m_fRadius;            // arc ball's radius in screen coords
    float m_fRadiusTranslation; // arc ball's radius for translating the target

    XMVECTOR m_qDown; // Quaternion before button down
    XMVECTOR m_qNow;  // Composite quaternion for current drag
    bool m_bDrag;     // Whether user is dragging arc ball

    POINT m_ptLastMouse;   // position of last mouse POINT
    XMVECTOR m_vDownPt;    // starting POINT of rotation arc
    XMVECTOR m_vCurrentPt; // current POINT of rotation arc

    XMVECTOR ScreenToVector(float fScreenPtX, float fScreenPtY);
};

//--------------------------------------------------------------------------------------
// used by CCamera to map WM_KEYDOWN keys
//--------------------------------------------------------------------------------------
enum D3DUtil_CameraKeys
{
    CAM_STRAFE_LEFT = 0,
    CAM_STRAFE_RIGHT,
    CAM_MOVE_FORWARD,
    CAM_MOVE_BACKWARD,
    CAM_MOVE_UP,
    CAM_MOVE_DOWN,
    CAM_RESET,
    CAM_CONTROLDOWN,
    CAM_MAX_KEYS,
    CAM_UNKNOWN = 0xFF
};

#define KEY_WAS_DOWN_MASK 0x80
#define KEY_IS_DOWN_MASK 0x01

#define MOUSE_LEFT_BUTTON 0x01
#define MOUSE_MIDDLE_BUTTON 0x02
#define MOUSE_RIGHT_BUTTON 0x04
#define MOUSE_WHEEL 0x08

//--------------------------------------------------------------------------------------
// Simple base camera class that moves and rotates.  The base class
//       records mouse and keyboard input for use by a derived class, and
//       keeps common state.
//--------------------------------------------------------------------------------------
class CBaseCamera
{
public:
    CBaseCamera();

    // Call these from client and use Get*Matrix() to read new matrices
    virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void FrameMove(float fElapsedTime) = 0;

    // Functions to change camera matrices
    virtual void Reset();
    virtual void SetViewParams(XMVECTOR vEyePt, XMVECTOR vLookatPt);
    virtual void SetProjParams(float fFOV, float fAspect, float fNearPlane, float fFarPlane);

    // Functions to change behavior
    virtual void SetDragRect(RECT &rc)
    {
        m_rcDrag = rc;
    }
    void SetInvertPitch(bool bInvertPitch)
    {
        m_bInvertPitch = bInvertPitch;
    }
    void SetDrag(bool bMovementDrag, float fTotalDragTimeToZero = 0.25f)
    {
        m_bMovementDrag = bMovementDrag;
        m_fTotalDragTimeToZero = fTotalDragTimeToZero;
    }
    void SetEnableYAxisMovement(bool bEnableYAxisMovement)
    {
        m_bEnableYAxisMovement = bEnableYAxisMovement;
    }
    void SetEnablePositionMovement(bool bEnablePositionMovement)
    {
        m_bEnablePositionMovement = bEnablePositionMovement;
    }
    void SetClipToBoundary(bool bClipToBoundary, const XMVECTOR *pvMinBoundary,
                           const XMVECTOR *pvMaxBoundary)
    {
        m_bClipToBoundary = bClipToBoundary;
        if (pvMinBoundary)
            m_vMinBoundary = *pvMinBoundary;
        if (pvMaxBoundary)
            m_vMaxBoundary = *pvMaxBoundary;
    }
    void SetScalers(float fRotationScaler = 0.01f, float fMoveScaler = 5.0f)
    {
        m_fRotationScaler = fRotationScaler;
        m_fMoveScaler = fMoveScaler;
    }
    void SetNumberOfFramesToSmoothMouseData(int nFrames)
    {
        if (nFrames > 0)
            m_fFramesToSmoothMouseData = (float)nFrames;
    }
    void SetResetCursorAfterMove(bool bResetCursorAfterMove)
    {
        m_bResetCursorAfterMove = bResetCursorAfterMove;
    }

    // Functions to get state
    XMMATRIX GetViewMatrix() const
    {
        return m_mView;
    }
    XMMATRIX GetProjMatrix() const
    {
        return m_mProj;
    }
    XMVECTOR GetEyePt() const
    {
        return m_vEye;
    }
    XMVECTOR GetLookAtPt() const
    {
        return m_vLookAt;
    }
    float GetNearClip() const
    {
        return m_fNearPlane;
    }
    float GetFarClip() const
    {
        return m_fFarPlane;
    }

    bool IsBeingDragged() const
    {
        return (m_bMouseLButtonDown || m_bMouseMButtonDown || m_bMouseRButtonDown);
    }
    bool IsMouseLButtonDown() const
    {
        return m_bMouseLButtonDown;
    }
    bool IsMouseMButtonDown() const
    {
        return m_bMouseMButtonDown;
    }
    bool IsMouseRButtonDown() const
    {
        return m_bMouseRButtonDown;
    }

protected:
    // Functions to map a WM_KEYDOWN key to a D3DUtil_CameraKeys enum
    virtual D3DUtil_CameraKeys MapKey(UINT nKey);
    bool IsKeyDown(BYTE key) const
    {
        return ((key & KEY_IS_DOWN_MASK) == KEY_IS_DOWN_MASK);
    }
    bool WasKeyDown(BYTE key) const
    {
        return ((key & KEY_WAS_DOWN_MASK) == KEY_WAS_DOWN_MASK);
    }

    void ConstrainToBoundary(XMVECTOR &pV);
    void UpdateMouseDelta();
    void UpdateVelocity(float fElapsedTime);
    void GetInput(bool bGetKeyboardInput, bool bGetMouseInput, bool bGetGamepadInput,
                  bool bResetCursorAfterMove);

    XMMATRIX m_mView; // View matrix
    XMMATRIX m_mProj; // Projection matrix

    XMVECTOR m_vGamePadLeftThumb;
    XMVECTOR m_vGamePadRightThumb;

    int m_cKeysDown;                  // Number of camera keys that are down.
    BYTE m_aKeys[CAM_MAX_KEYS];       // State of input - KEY_WAS_DOWN_MASK|KEY_IS_DOWN_MASK
    XMVECTOR m_vKeyboardDirection;    // Direction vector of keyboard input
    POINT m_ptLastMousePosition;      // Last absolute position of mouse cursor
    bool m_bMouseLButtonDown;         // True if left button is down
    bool m_bMouseMButtonDown;         // True if middle button is down
    bool m_bMouseRButtonDown;         // True if right button is down
    int m_nCurrentButtonMask;         // mask of which buttons are down
    int m_nMouseWheelDelta;           // Amount of middle wheel scroll (+/-)
    XMVECTOR m_vMouseDelta;           // Mouse relative delta smoothed over a few frames
    float m_fFramesToSmoothMouseData; // Number of frames to smooth mouse data over

    XMVECTOR m_vDefaultEye;    // Default camera eye position
    XMVECTOR m_vDefaultLookAt; // Default LookAt position
    XMVECTOR m_vEye;           // Camera eye position
    XMVECTOR m_vLookAt;        // LookAt position
    float m_fCameraYawAngle;   // Yaw angle of camera
    float m_fCameraPitchAngle; // Pitch angle of camera

    RECT m_rcDrag;                // Rectangle within which a drag can be initiated.
    XMVECTOR m_vVelocity;         // Velocity of camera
    bool m_bMovementDrag;         // If true, then camera movement will slow to a stop otherwise movement is instant
    XMVECTOR m_vVelocityDrag;     // Velocity drag force
    float m_fDragTimer;           // Countdown timer to apply drag
    float m_fTotalDragTimeToZero; // Time it takes for velocity to go from full to 0
    XMVECTOR m_vRotVelocity;      // Velocity of camera

    float m_fFOV;       // Field of view
    float m_fAspect;    // Aspect ratio
    float m_fNearPlane; // Near plane
    float m_fFarPlane;  // Far plane

    float m_fRotationScaler; // Scaler for rotation
    float m_fMoveScaler;     // Scaler for movement

    bool m_bInvertPitch;            // Invert the pitch axis
    bool m_bEnablePositionMovement; // If true, then the user can translate the camera/model
    bool m_bEnableYAxisMovement;    // If true, then camera can move in the y-axis

    bool m_bClipToBoundary;  // If true, then the camera will be clipped to the boundary
    XMVECTOR m_vMinBoundary; // Min POINT in clip boundary
    XMVECTOR m_vMaxBoundary; // Max POINT in clip boundary

    bool m_bResetCursorAfterMove; // If true, the class will reset the cursor position so that the cursor always has space to move
};

//--------------------------------------------------------------------------------------
// Simple first person camera class that moves and rotates.
//       It allows yaw and pitch but not roll.  It uses WM_KEYDOWN and
//       GetCursorPos() to respond to keyboard and mouse input and updates the
//       view matrix based on input.
//--------------------------------------------------------------------------------------
class CFirstPersonCamera : public CBaseCamera
{
public:
    CFirstPersonCamera();

    // Call these from client and use Get*Matrix() to read new matrices
    virtual void FrameMove(float fElapsedTime);

    // Functions to change behavior
    void SetRotateButtons(bool bLeft, bool bMiddle, bool bRight, bool bRotateWithoutButtonDown = false);

    // Functions to get state
    XMMATRIX GetWorldMatrix()
    {
        return m_mCameraWorld;
    }

    XMVECTOR GetWorldRight() const
    {
        return m_mCameraWorld.r[0];
    }
    XMVECTOR GetWorldUp() const
    {
        return m_mCameraWorld.r[1];
    }
    XMVECTOR GetWorldAhead() const
    {
        return m_mCameraWorld.r[2];
    }
    XMVECTOR GetEyePt() const
    {
        return m_mCameraWorld.r[3];
    }

protected:
    XMMATRIX m_mCameraWorld; // World matrix of the camera (inverse of the view matrix)

    int m_nActiveButtonMask; // Mask to determine which button to enable for rotation
    bool m_bRotateWithoutButtonDown;
};

//--------------------------------------------------------------------------------------
// Simple model viewing camera class that rotates around the object.
//--------------------------------------------------------------------------------------
class CModelViewerCamera : public CBaseCamera
{
public:
    CModelViewerCamera();

    // Call these from client and use Get*Matrix() to read new matrices
    virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void FrameMove(float fElapsedTime);

    // Functions to change behavior
    virtual void SetDragRect(RECT &rc);
    void Reset();
    void SetViewParams(XMVECTOR vEyePt, XMVECTOR vLookatPt);
    void SetButtonMasks(int nRotateModelButtonMask = MOUSE_LEFT_BUTTON, int nZoomButtonMask = MOUSE_WHEEL,
                        int nRotateCameraButtonMask = MOUSE_RIGHT_BUTTON)
    {
        m_nRotateModelButtonMask = nRotateModelButtonMask, m_nZoomButtonMask = nZoomButtonMask;
        m_nRotateCameraButtonMask = nRotateCameraButtonMask;
    }
    void SetAttachCameraToModel(bool bEnable = false)
    {
        m_bAttachCameraToModel = bEnable;
    }
    void SetWindow(int nWidth, int nHeight, float fArcballRadius = 0.9f)
    {
        m_WorldArcBall.SetWindow(nWidth, nHeight, fArcballRadius);
        m_ViewArcBall.SetWindow(nWidth, nHeight, fArcballRadius);
    }
    void SetRadius(float fDefaultRadius = 5.0f, float fMinRadius = 1.0f, float fMaxRadius = FLT_MAX)
    {
        m_fDefaultRadius = m_fRadius = fDefaultRadius;
        m_fMinRadius = fMinRadius;
        m_fMaxRadius = fMaxRadius;
        m_bDragSinceLastUpdate = true;
    }
    void SetModelCenter(XMVECTOR vModelCenter)
    {
        m_vModelCenter = vModelCenter;
    }
    void SetLimitPitch(bool bLimitPitch)
    {
        m_bLimitPitch = bLimitPitch;
    }
    void SetViewQuat(XMVECTOR q)
    {
        m_ViewArcBall.SetQuatNow(q);
        m_bDragSinceLastUpdate = true;
    }
    void SetWorldQuat(XMVECTOR q)
    {
        m_WorldArcBall.SetQuatNow(q);
        m_bDragSinceLastUpdate = true;
    }

    // Functions to get state
    XMMATRIX GetWorldMatrix() const
    {
        return m_mWorld;
    }
    void SetWorldMatrix(XMMATRIX &mWorld)
    {
        m_mWorld = mWorld;
        m_bDragSinceLastUpdate = true;
    }

protected:
    CD3DArcBall m_WorldArcBall;
    CD3DArcBall m_ViewArcBall;
    XMVECTOR m_vModelCenter;
    XMMATRIX m_mModelLastRot; // Last arcball rotation matrix for model
    XMMATRIX m_mModelRot;     // Rotation matrix of model
    XMMATRIX m_mWorld;        // World matrix of model

    int m_nRotateModelButtonMask;
    int m_nZoomButtonMask;
    int m_nRotateCameraButtonMask;

    bool m_bAttachCameraToModel;
    bool m_bLimitPitch;
    float m_fRadius;             // Distance from the camera to model
    float m_fDefaultRadius;      // Distance from the camera to model
    float m_fMinRadius;          // Min radius
    float m_fMaxRadius;          // Max radius
    bool m_bDragSinceLastUpdate; // True if mouse drag has happened since last time FrameMove is called.

    XMMATRIX m_mCameraRotLast;
};
