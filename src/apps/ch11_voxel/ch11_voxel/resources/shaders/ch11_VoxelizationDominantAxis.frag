#version 460


#include "ch11_VoxelsSsbo.glsl"

in vec3 ex_Position_grid;


void main(void)
{
	// Truncates toward zero, as needed (behaves as floor in Z+)
	ivec3 voxel = ivec3(ex_Position_grid);
	markOccupiedAtomic(voxel);
}
