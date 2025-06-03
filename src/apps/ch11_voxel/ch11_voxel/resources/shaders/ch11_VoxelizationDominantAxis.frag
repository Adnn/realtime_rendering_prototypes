#version 460


#include "ch11_VoxelsSsbo.glsl"


void main(void)
{
	// TODO: interpolate the arithmetic in floor() from the vertex stage
	// but we could not use gl_FragCoord for that
	ivec3 voxel = ivec3(ivec2(gl_FragCoord.xy),
				        floor((1 - gl_FragCoord.z) * ub_GridDimension));

	markOccupiedAtomic(voxel);
}
