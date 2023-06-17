/* 
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved. 
* 
* NVIDIA CORPORATION and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto. Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an express 
* license agreement from NVIDIA CORPORATION is strictly prohibited. 
*/ 

char const g_VoxelizationPS[] = R"(
void main(float4 position: SV_Position, VxgiVoxelizationPSInputData vxgiData)
{
	VxgiStoreVoxelizationData(vxgiData, float3(1.0, 1.0, 1.0));
}
)";
