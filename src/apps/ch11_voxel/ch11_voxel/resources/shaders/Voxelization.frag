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
	vec2 slice = floor(ex_Uv * u_GridSide);
	const uint voxelPerUint = 4; // Cpp uint8_t per GLSL uint
	uint xStride = u_GridSide / voxelPerUint;
	uint yStride = xStride * u_GridSide;
	
	unsigned int idx = int(xStride * slice.x + yStride * slice.y);

	// TODO: handle depth
	ub_Voxels[idx] = 1 + (1 << 16);
}
