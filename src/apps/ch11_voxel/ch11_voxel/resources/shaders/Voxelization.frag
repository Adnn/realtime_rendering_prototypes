#version 460


in vec2 ex_Uv;

uniform uint u_GridSide;

layout(std430, binding = 10) buffer VoxelsSsbo
{
  uint ub_Voxels[];
};


void main(void)
{
	// floor() because the float mulitplication exact value would be "idx.5",
	// which would be rounded up because of the "0.5".
	ivec2 slice = ivec2(floor(ex_Uv * u_GridSide));
	const uint voxelPerUint = 4; // Cpp uint8_t per GLSL uint
	uint xStride = u_GridSide / voxelPerUint;
	uint yStride = xStride * u_GridSide;
	
	unsigned int idx = xStride * slice.x + yStride * slice.y;

	// TODO: handle depth
	// For even(odd) grid position, set voxel at even(odd) depth
	uint parity = (slice.x + slice.y) % 2;
	ub_Voxels[idx] = (1 + (1 << 16)) << (parity * 8);
}
