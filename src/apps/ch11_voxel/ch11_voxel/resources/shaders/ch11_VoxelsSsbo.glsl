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
	// Debug access: show a 3D checkerboard
	//return (aVoxel.x + aVoxel.y + aVoxel.z) % 2;

	uint idx = getVoxelTableIdx(aVoxel);
	return (ub_Voxels[idx] >> ((aVoxel.z % gVoxelPerUint) * 8)) & 0xFF;
}


void setVoxelValue(ivec3 aVoxel, uint aValue)
{
	uint idx = getVoxelTableIdx(aVoxel);
	uint written = (aValue & 0xFF) << ((aVoxel.z % gVoxelPerUint) * 8);
	ub_Voxels[idx] = written;
}


#endif //VOXELSSSBO_GLSL_INCLUDE_GUARD
