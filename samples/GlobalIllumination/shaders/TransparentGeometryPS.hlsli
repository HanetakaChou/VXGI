/* 
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved. 
* 
* NVIDIA CORPORATION and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto. Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an express 
* license agreement from NVIDIA CORPORATION is strictly prohibited. 
*/ 

char const g_TransparentGeometryPS[] = R"(struct PSInput
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
    float3 positionWS : WSPOSITION;
};

cbuffer GlobalConstants : register(b0)
{
    float4x4 g_WorldMatrix;
    float4x4 g_ViewProjMatrix;
    float4x4 g_ViewProjMatrixInv;
    float4x4 g_LightViewProjMatrix;
    float4 g_CameraPos;
    float4 g_LightDirection;
    float4 g_DiffuseColor;
    float4 g_LightColor;
    float4 g_AmbientColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
    float g_TransparentRoughness;
    float g_TransparentReflectance;
};

static const float PI = 3.14159265;

float Luminance(float3 color)
{
    return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
}

float3 ConvertToLDR(float3 color)
{
    float srcLuminance = Luminance(color);

    float sqrWhiteLuminance = 50;
    float scaledLuminance = srcLuminance * 8;
    float mappedLuminance = (scaledLuminance * (1 + scaledLuminance / sqrWhiteLuminance)) / (1 + scaledLuminance);

    return color * (mappedLuminance / srcLuminance);
}

[earlydepthstencil]
float4 main(PSInput input, bool isFront: SV_IsFrontFace) : SV_Target0
{
    float3 irradiance = 0;
    float3 normal = normalize(input.normal);
    float reflectionFactor = g_TransparentReflectance;

    VxgiConeTracingArguments args = VxgiDefaultConeTracingArguments();

    float roughness = g_TransparentRoughness;
    float alpha = max(0.01, roughness * roughness);
    args.coneFactor = alpha * sqrt(PI);
    args.maxTracingDistance = 500;

    float3 viewVector = normalize(input.positionWS - g_CameraPos.xyz);

    float cosViewAngle = saturate(dot(-viewVector.xyz, normal.xyz));
    float fresnel = pow(1 - cosViewAngle, 5);
    reflectionFactor += (1 - reflectionFactor) * fresnel;

    if(reflectionFactor > 0)
    {
      float3 reflectedVector = normalize(reflect(viewVector, normal.xyz));
      args.direction = reflectedVector;
      args.firstSampleT = VxgiGetMinSampleSizeInVoxels(input.positionWS) * 5;
      args.firstSamplePosition = input.positionWS + args.direction * VxgiGetFinestVoxelSize() * args.firstSampleT;

      VxgiConeTracingResults reflectedCone = VxgiTraceCone(args);
      irradiance = reflectedCone.irradiance;
    }

    if(reflectionFactor < 1)
    {
      float refractionIndex = 1.330;
      float3 refractedVector = refract(viewVector, normal.xyz, isFront ? rcp(refractionIndex) : refractionIndex);

      if(any(refractedVector))
      {
        refractedVector = normalize(refractedVector);
        args.direction = refractedVector;
        args.firstSampleT = VxgiGetMinSampleSizeInVoxels(input.positionWS) * 10;
        args.firstSamplePosition = input.positionWS + args.direction * VxgiGetFinestVoxelSize() * args.firstSampleT;

        VxgiConeTracingResults refractedCone = VxgiTraceCone(args);
        irradiance = lerp(refractedCone.irradiance, irradiance, reflectionFactor);
      }
    }

    float3 color = irradiance / (PI * alpha * alpha);
    color = ConvertToLDR(color);

    return float4(color, 1);
})";
