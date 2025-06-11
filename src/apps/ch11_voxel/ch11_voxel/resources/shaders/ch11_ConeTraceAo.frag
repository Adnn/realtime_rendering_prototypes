#version 460


#include "ch11_VoxelsSsbo.glsl"
#include "shaders/Helpers.glsl"


in vec3 ex_Normal_world;
in vec3 ex_Position_world;

uniform sampler3D u_VoxelsAlbedoTexture;

uniform float u_TanHalfAperture;

uniform float u_VoxelSize;
uniform vec3 u_AabbMin;
out vec4 out_Color;


float traceCone(vec3 position_aabb, vec3 normal_aabb, float tanHalfAngle)
{
	// TODO: check reference implementation
	const float maxDistance = 2;

	// TODO (I think this is beta in the explanation)
	const float samplingFactor = 1;

	// TODO determine good range
	const float aoFalloff = 1;
	float falloff = 0.5f * aoFalloff / u_VoxelSize;

	vec3 direction_aabb = normal_aabb;
	vec3 startPosition_aabb = position_aabb + normal_aabb * u_VoxelSize; 
	//vec3 startPosition = position_aabb; // or grid centered?

	// Distance marched along the cone, in world unit
	float t = u_VoxelSize; // Offset to limit self-sampling

	// ambient occlusion
	float occlusion = 0;

	while(occlusion < 1.0f && t <= maxDistance)
	{
		float coneDiameter = 2 * t * tanHalfAngle;
		float mipLevel = log2(coneDiameter / u_VoxelSize);

		vec3 samplePosition_aabb = startPosition_aabb + direction_aabb * t;
		vec3 position_uvw = samplePosition_aabb / (u_VoxelSize * ub_GridDimension);
		vec4 albedo = textureLod(u_VoxelsAlbedoTexture, position_uvw, mipLevel);

		// TODO: actual irradiance marching
		// front to back
		//coneSample += (1.0f - coneSample.a) * albedo;

		occlusion += ((1.0f - occlusion) * albedo.a) / (1.0f + falloff * coneDiameter);
		// march the cone
		t += coneDiameter * samplingFactor;
	}

	return occlusion;
}


void main(void)
{
	out_Color = vec4(mapToRgb(ex_Normal_world), 1);
	//return;

	// Sample the 3D texture at the fragment position
	{
		// TODO: determine uvw at vertex level
		vec3 position_aabb = ex_Position_world - u_AabbMin;
		//ivec3 position_voxel = ivec3(position_aabb / u_VoxelSize);
		//vec3 uvw = position_voxel / vec3(ub_GridDimension);
		vec3 uvw = (position_aabb / u_VoxelSize) / vec3(ub_GridDimension);
		float occupancy = texture(u_VoxelsAlbedoTexture, uvw).a;

		out_Color = vec4(vec3(occupancy), 1);
	}

	{
		vec3 position_aabb = ex_Position_world - u_AabbMin;
		vec3 normal_world = normalize(ex_Normal_world);

		// Note: The AABB is aligned on world axis, so normal in world base
		// is the same direction in AABB.
		float occlusion = traceCone(position_aabb, normal_world, u_TanHalfAperture);

		out_Color = vec4(vec3(1-occlusion), 1);
	}
}
