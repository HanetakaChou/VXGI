/* 
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved. 
* 
* NVIDIA CORPORATION and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto. Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an express 
* license agreement from NVIDIA CORPORATION is strictly prohibited. 
*/ 

#define VXGI_DYNAMIC_LOAD_LIBRARY 1
#include "GFSDK_VXGI.h"


#define VXGI_DEFINE_FN(name) PFN_##name name = nullptr
#define VXGI_LOAD_FN(name) name = (PFN_##name)GetProcAddress(hDLL, #name); if(name == nullptr) return Status::FUNCTION_MISSING

namespace VXGI
{
    VXGI_DEFINE_FN(VFX_VXGI_CreateGIObject);
    VXGI_DEFINE_FN(VFX_VXGI_DestroyGIObject);
    VXGI_DEFINE_FN(VFX_VXGI_CreateShaderCompiler);
    VXGI_DEFINE_FN(VFX_VXGI_DestroyShaderCompiler);
    VXGI_DEFINE_FN(VFX_VXGI_VerifyInterfaceVersion);
    VXGI_DEFINE_FN(VFX_VXGI_GetInternalShaderHash);
    VXGI_DEFINE_FN(VFX_VXGI_ComputeVoxelizationViewParameters);
    VXGI_DEFINE_FN(VFX_VXGI_StatusToString);

    Status::Enum GetProcAddresses(HMODULE hDLL)
    {
        if(hDLL == 0 || hDLL == INVALID_HANDLE_VALUE)
            return Status::NULL_ARGUMENT;

        VXGI_LOAD_FN(VFX_VXGI_CreateGIObject);
        VXGI_LOAD_FN(VFX_VXGI_DestroyGIObject);
        VXGI_LOAD_FN(VFX_VXGI_CreateShaderCompiler);
        VXGI_LOAD_FN(VFX_VXGI_DestroyShaderCompiler);
        VXGI_LOAD_FN(VFX_VXGI_VerifyInterfaceVersion);
        VXGI_LOAD_FN(VFX_VXGI_GetInternalShaderHash);
        VXGI_LOAD_FN(VFX_VXGI_ComputeVoxelizationViewParameters);
        VXGI_LOAD_FN(VFX_VXGI_StatusToString);

        return Status::OK;
    }
}