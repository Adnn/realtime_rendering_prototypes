#version 460


#include "ch11_VoxelsSsbo.glsl"


in vec3 ex_Position_view;

uniform float u_AabbDepth;


void main(void)
{
	// TODO: interpolate the arithmetic in floor() from the vertex shader
	uint z = uint(floor(-ex_Position_view.z * ub_GridDimension / u_AabbDepth));
	ivec3 voxel = ivec3( ivec2(gl_FragCoord.xy),
				         max(0, (ub_GridDimension - 1) - z) );

	setVoxelValue(voxel, 1);
}
