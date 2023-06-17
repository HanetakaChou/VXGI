char const g_VoxelizationPS[] = R"(flat in VxgiVoxelizationPSInputData vxgiData;

void main()
{
	VxgiStoreVoxelizationData(vxgiData, float3(1.0, 1.0, 1.0));
})";
