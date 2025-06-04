#version 460


#include "ch11_VoxelsSsbo.glsl"

in vec3 ex_Position_grid;

uniform bool u_ConservativeDepthRange;

void main(void)
{
	if(u_ConservativeDepthRange)
	{
		float dzdx = dFdx(ex_Position_grid.z);
		float dzdy = dFdy(ex_Position_grid.z);
		// The maximal amplitude of the depth variation within the quad
		float dz = abs(dzdx) + abs(dzdy);

		// Minimal Z-value over the fragment coverage
		// TODO: Is it the minimal value (corner) or the side value (half-fragment displacement)?
		float zMin = ex_Position_grid.z - 0.5 * dz;
		//float zMin = fma(-0.5, dz, ex_Position_grid.z);
		float zMax = ex_Position_grid.z + 0.5 * dz;

		// Truncate the floats toward zero (equivalent to floor on Z+)
		for (int z = int(zMin); z <= int(zMax); ++z)
		{
			markOccupiedAtomic(ivec3(ivec2(ex_Position_grid.xy), z));
		}
	}
	else
	{
		// Truncates toward zero, as needed (equivalent to floor in Z+)
		ivec3 voxel = ivec3(ex_Position_grid);
		markOccupiedAtomic(voxel);
	}
}
