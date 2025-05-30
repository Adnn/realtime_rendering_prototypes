#if !defined(VOXELSSSBO_GLSL_INCLUDE_GUARD)
#define VOXELSSSBO_GLSL_INCLUDE_GUARD


const uint gVoxelPerUint = 4; // Cpp uint8_t per GLSL uint


layout(std430, binding = 10) buffer VoxelsSsbo
{
	uint ub_GridDimension;
	uint ub_Voxels[];
};

uint getVoxelTableIdx(ivec3 aVoxel)
{
	uint xStride = ub_GridDimension / gVoxelPerUint;
	uint yStride = xStride * ub_GridDimension;
	return xStride * aVoxel.x 
		   + yStride * aVoxel.y 
		   + uint(floor(aVoxel.z / gVoxelPerUint))
		   ;
}


uint getVoxelValue(ivec3 aVoxel)
{
	//#define GET_VOXEL_CHECKERBOARD
	#if defined(GET_VOXEL_CHECKERBOARD)
		// Debug access: show a 3D checkerboard
		return (aVoxel.x + aVoxel.y + aVoxel.z) % 2;
	#else
		uint idx = getVoxelTableIdx(aVoxel);
		return (ub_Voxels[idx] >> ((aVoxel.z % gVoxelPerUint) * 8)) & 0xFF;
	#endif
}


void setVoxelValue(ivec3 aVoxel, uint aValue)
{
	uint idx = getVoxelTableIdx(aVoxel);
	// Get the existing value
	uint bitoffset = (aVoxel.z % gVoxelPerUint) * 8;
	uint mask = ~(0xFF << bitoffset);
	uint value = (aValue & 0xFF) << bitoffset;
	uint written = (ub_Voxels[idx] & mask) | value;
	ub_Voxels[idx] = written;

	//uint idx = getVoxelTableIdx(aVoxel);
	//ub_Voxels[idx] = 0x01010101;
}


#endif //VOXELSSSBO_GLSL_INCLUDE_GUARD
