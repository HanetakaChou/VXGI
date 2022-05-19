/*
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// UnrealEngine-NVIDIA-Fork/Engine/Shaders/VXGIGBufferAccess.usf

struct GBufferParameters
{
    VXGI::float4x4 viewMatrixInv;
    VXGI::float4x4 projMatrixInv;
    VXGI::float2 viewportOrigin;
    VXGI::float2 viewportSizeInv;
};

struct BuiltinGBufferParameters
{
    GBufferParameters g_GBuffer;
    GBufferParameters g_PreviousGBuffer;
};

#define CBV_SLOT_BUILTIN_GBUFFER_PARAMETERS 0
#define SRV_SLOT_DEPTH_BUFFER 0
#define SRV_SLOT_DEPTH_BUFFER_PREV 1
#define SRV_SLOT_NORMAL_BUFFER 2
#define SRV_SLOT_NORMAL_BUFFER_PREV 3

char const g_GBufferLoader[] = R"(
struct GBufferParameters
{
    float4x4 viewMatrixInv;
    float4x4 projMatrixInv;
    float2 viewportOrigin;
    float2 viewportSizeInv;
};

cbuffer cBuiltinGBufferParameters : register(b0)
{
    GBufferParameters g_GBuffer;
    GBufferParameters g_PreviousGBuffer;
}

Texture2D g_Depth : register(t0);
Texture2D g_DepthPrev : register(t1);
Texture2D g_GBufferA : register(t2);
Texture2D g_GBufferAPrev : register(t3);

// A structure that you can use to keep any information in - it's passed from VxgiLoadGBufferSample to all other functions
struct VxgiGBufferSample
{
    float viewDepth;
    float3 worldPos;
    float3 normal;
    float3 cameraPos;
    float3 viewRight;
    float3 viewUp;
    float roughness;
};

// Loads a sample from the current or previous G-buffer at given window coordinates.
// If onlyPosition is true, the function should skip loading and processing anything else, for performance.
bool VxgiLoadGBufferSample(float2 windowPos, uint viewIndex, bool previous, bool onlyPosition, out VxgiGBufferSample result)
{
    float4x4 viewMatrixInv;
    float4x4 projMatrixInv;
    float2 viewportOrigin;
    float2 viewportSizeInv;
    float4 GBufferA;
    float Depth;
    [branch] 
    if (previous)
    {
        viewMatrixInv = g_PreviousGBuffer.viewMatrixInv;
        projMatrixInv = g_PreviousGBuffer.projMatrixInv;
        viewportOrigin = g_PreviousGBuffer.viewportOrigin;
        viewportSizeInv = g_PreviousGBuffer.viewportSizeInv;
        GBufferA = g_GBufferAPrev[int2(windowPos)];
        Depth = g_DepthPrev[int2(windowPos)].r;
    }
    else
    {
        viewMatrixInv = g_GBuffer.viewMatrixInv;
        projMatrixInv = g_GBuffer.projMatrixInv;
        viewportOrigin = g_GBuffer.viewportOrigin;
        viewportSizeInv = g_GBuffer.viewportSizeInv;
        GBufferA = g_GBufferA[int2(windowPos)];
        Depth = g_Depth[int2(windowPos)].r;
    }
    
    float2 UV = (windowPos - viewportOrigin) * viewportSizeInv;
    float4 clipPos = float4(UV.x * 2.0 - 1.0, 1.0 - UV.y * 2.0, Depth, 1.0);
    [branch]
    if (any(abs(clipPos.xyz) > abs(clipPos.w)))
    {
        return false;
    }
    
    float4 viewPosV4 = mul(clipPos, projMatrixInv);
    float3 viewPos = viewPosV4.xyz /= viewPosV4.w;
    result.viewDepth = viewPos.z;

    result.worldPos = mul(float4(viewPos, 1.0), viewMatrixInv).xyz;

    result.normal = GBufferA.xyz;

    result.cameraPos = mul(float4(0.0, 0.0, 0.0, 1.0), viewMatrixInv).xyz;
    result.viewRight = mul(float4(1.0, 0.0, 0.0, 0.0), viewMatrixInv).xyz;
    result.viewUp = mul(float4(0.0, 1.0, 0.0, 0.0), viewMatrixInv).xyz;

    result.roughness = GBufferA.w;

    return true;
}

// Returns the view depth or distance to camera for a given sample
float VxgiGetGBufferViewDepth(VxgiGBufferSample gbufferSample)
{
    return gbufferSample.viewDepth;
}

// Returns the world position of a given sample
float3 VxgiGetGBufferWorldPos(VxgiGBufferSample gbufferSample)
{
    return gbufferSample.worldPos;
}

// Returns the world space normal for a given sample
float3 VxgiGetGBufferNormal(VxgiGBufferSample gbufferSample)
{
    return gbufferSample.normal;
}

// Returns the world space tangent for a given sample.
// Tangents are used by VXGI to calculate directions for diffuse cones. Using tangents that originate from meshes
// can help avoid seams on diffuse illumination where computed tangents have a discontinuity.
float3 VxgiGetGBufferTangent(VxgiGBufferSample gbufferSample)
{
    float3 normal = gbufferSample.normal;
    
    float3 absNormal = abs(normal);
    float maxComp = max(absNormal.x, max(absNormal.y, absNormal.z));
        
    float3 tangent;
    [branch] 
    if (maxComp == absNormal.x)
    {
        tangent = float3((-normal.y - normal.z) * sign(normal.x), absNormal.x, absNormal.x);
    }
    else if (maxComp == absNormal.y)
    {
        tangent = float3(absNormal.y, (-normal.x - normal.z) * sign(normal.y), absNormal.y);
    }
    else
    {
        tangent = float3(absNormal.z, absNormal.z, (-normal.x - normal.y) * sign(normal.z));
    }
        
    return (normalize(tangent));
}

// Returns the diffuse/ambient tracing settings for the current sample.
VxgiDiffuseShaderParameters VxgiGetGBufferDiffuseShaderParameters(VxgiGBufferSample gbufferSample)
{
    // An instance of this structure with all-default fields can be obtained by calling VxgiGetDefaultDiffuseShaderParameters().
    return VxgiGetDefaultDiffuseShaderParameters();
}

// Returns the specular tracing settings for the current sample.
VxgiSpecularShaderParameters VxgiGetGBufferSpecularShaderParameters(VxgiGBufferSample gbufferSample)
{
    VxgiSpecularShaderParameters result = VxgiGetDefaultSpecularShaderParameters();
    result.roughness = min(gbufferSample.roughness, 0.75);
    result.viewDirection = normalize(gbufferSample.worldPos - gbufferSample.cameraPos);
    return result;
}

// Returns the area light settings for the current sample.
VxgiAreaLightShaderParameters VxgiGetGBufferAreaLightShaderParameters(VxgiGBufferSample gbufferSample)
{
    VxgiAreaLightShaderParameters result = VxgiGetDefaultAreaLightShaderParameters();
    result.roughness = gbufferSample.roughness;
    result.viewDirection = normalize(gbufferSample.worldPos - gbufferSample.cameraPos);
    return result;
}

// Return true if SSAO should be computed for the sample;
// VXGI assumes that the sample is valid (position, normal etc.) if this function returns true.
bool VxgiGetGBufferEnableSSAO(VxgiGBufferSample gbufferSample)
{
    return false;
}

// Returns the clip-space position for a given sample.
// When normalized == true, VXGI expects that the .w component of position will be 1.0
// This function is only used for SSAO.
float4 VxgiGetGBufferClipPos(VxgiGBufferSample gbufferSample, bool normalized)
{
    return float4(0.0, 0.0, 0.0, 0.0);
}

// Maps a given clip-space position to window coordinates which can be used to sample the G-buffer.
// This function is only used for SSAO.
float2 VxgiGBufferMapClipToWindow(uint viewIndex, float2 clipPos)
{
    return float2(0.0, 0.0);
}

float2 VxgiGBufferMapWindowToClip(uint viewIndex, float2 windowPos)
{
    float2 UV = (windowPos - g_GBuffer.viewportOrigin.xy) * g_GBuffer.viewportSizeInv.xy;

	float2 clipPos;
	clipPos.x = UV.x * 2 - 1;
	clipPos.y = 1 - UV.y * 2;
	return clipPos;
}

// Maps a given sample to a different view in the same or previous frame and returns the window coordinates.
// Returns true if a matching surface exists in the other view, false otherwise.
bool VxgiGetGBufferPositionInOtherView(VxgiGBufferSample gbufferSample, uint viewIndex, bool previous, out float2 prevWindowPos)
{
    prevWindowPos = float2(0.0, 0.0);
    return false;
}

// Returns irradiance from an environment map for a surface at 'surfacePos' coming from a cone
// looking in 'coneDirection' with the cone factor 'coneFactor'.
float3 VxgiGetEnvironmentIrradiance(float3 surfacePos, float3 coneDirection, float coneFactor, bool isSpecular)
{
    return float3(0.0, 0.0, 0.0);
}

void VxgiGetGBufferRightAndUp(VxgiGBufferSample gbufferSample, out float3 right, out float3 up)
{
    right = gbufferSample.viewRight;
    up = gbufferSample.viewUp;
}
)";