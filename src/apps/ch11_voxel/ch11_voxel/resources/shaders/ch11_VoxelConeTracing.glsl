#if !defined(VOXEL_CONE_TRACING_GLSL_INCLUDE_GUARD)
#define VOXEL_CONE_TRACING_GLSL_INCLUDE_GUARD


#include "ch11_VoxelsSsbo.glsl"

#include "shaders/Constants.glsl"


uniform bool u_GridAlign;

uniform sampler3D u_VoxelsAlbedoTexture;
uniform sampler3D u_VoxelsIrradianceTexture;


// Partition the hemisphere with 7 cones is convenient:
// with a half-angle of 30°, 1 cone pointing along the normal, and 5 ring cones
// All cones are touching their neighbors:
// In a vertical plane (e.g.XY), 3 cones are fitting (3 * 2*30 = 180)
// In the horizontal plane XZ, 6 cones are fitting (6 * 2*30 = 360)
//const vec3 diffuseConeAxes[] =
//{
//    vec3(  0.0f, 0.0f,  1.0f), // Along +Z
//    vec3(  0.0f,  0.866025f, 0.5f), // ring, toward +X
//    vec3( 0.75f,  0.433013f, 0.5f), 
//    vec3( 0.75f, -0.433013f, 0.5f),
//    vec3(  0.0f, -0.866025f, 0.5f), // ring, toward -X
//    vec3(-0.75f, -0.433013f, 0.5f),
//    vec3(-0.75f,  0.433013f, 0.5f), 
//};

// But usually found split in 6:
const vec3 gDiffuseConeDirections[] =
{
    vec3( 0.0f,		 0.0f,  1.0f),
    vec3( 0.0f,		  0.866025f,  0.5),
    vec3( 0.823639f,  0.267617f,  0.5),
    vec3( 0.509037f, -0.7006629f, 0.5),
    vec3(-0.50937f,  -0.7006629f, 0.5),
    vec3(-0.823639f,  0.267617f,  0.5)
};

const float gDiffuseConeWeights[] =
{
    M_PI / 4.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
};


/// @return The irradiance accumulated along the march in .rgb, the ambient occlusion in .a
vec4 traceCone(vec3 position_aabb, vec3 normal_aabb, 
			   vec3 coneAxis_aabb, float tanHalfAngle,
			   float aVoxelSize)
{
	// TODO: check reference implementation
	//const float maxDistance = 2;
	const float maxDistance = 10;

	// A factor to implement the potential difference between d and d' in Crassin's paper.
	// This is beta in the explanation here: https://github.com/jose-villegas/VCTRenderer?tab=readme-ov-file#4-voxel-cone-tracing
	const float samplingFactor = 1;

	// Initial offset, to mitigate self-sampling.
	// The factor will be applied to the voxel size.
	const float offsetFactor = 1;

	// TODO determine good range
	const float aoFalloff = 8;
	float falloff = 0.5f * aoFalloff / aVoxelSize;

	vec3 direction_aabb = coneAxis_aabb;
	if (u_GridAlign)
	{
		position_aabb = (floor(position_aabb / aVoxelSize) + vec3(0.5)) * aVoxelSize;
	}

	// Note: Some implementation offset in the direction of the normal instead of the cone
	// e.g. https://github.com/jose-villegas/VCTRenderer/blob/9ae0dbe5bd60e85514e3e582bf23f2868c6b51fc/engine/assets/shaders/light_pass.frag#L147
	vec3 startPosition_aabb = position_aabb + normal_aabb * offsetFactor * aVoxelSize; 

	// Distance marched along the cone, in world unit
	float t = 1.0 * aVoxelSize; // Another offset to limit self-sampling

	// ambient occlusion
	float occlusion = 0;
	vec4 marchedIrradiance = vec4(0);

	const ivec3 fragment_grid = ivec3(position_aabb / aVoxelSize);

	while(marchedIrradiance.a < 1.0f && t <= maxDistance)
	{
		float coneDiameter = 2 * t * tanHalfAngle;
		float mipLevel = log2(coneDiameter / aVoxelSize);

		vec3 samplePosition_aabb = startPosition_aabb + direction_aabb * t;

		const ivec3 sample_grid = ivec3(samplePosition_aabb / aVoxelSize);
		if(sample_grid == fragment_grid)
		{
			return vec4(1, 0, 1, 1);
		}

		vec3 position_uvw = samplePosition_aabb / (aVoxelSize * ub_GridDimension);
		// TODO: rename, this is not albedo but occupancy atm
		vec4 albedo = textureLod(u_VoxelsAlbedoTexture, position_uvw, mipLevel);
		vec4 irradianceSample = textureLod(u_VoxelsIrradianceTexture, position_uvw, mipLevel);

		// irradiance marching, front to back compositing
		//#define GPU_GEMS_BTF
		#if defined(GPU_GEMS_BTF)
			// see chapter 6 of GPU GEMS chapter 39 volume rendering techniques
			// https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-39-volume-rendering-techniques
			marchedIrradiance += (1.0f - marchedIrradiance.a) * irradianceSample;
		#else
			// It seems the Crassin paper back-to-front composition is wrong
			// the alpha should not be reapplied to previous occlusion
			marchedIrradiance.rgb += 
				(1 - marchedIrradiance.a) * irradianceSample.a * irradianceSample.rgb;
			marchedIrradiance.a += (1 - marchedIrradiance.a) * irradianceSample.a;
		#endif

		occlusion += ((1.0f - occlusion) * albedo.a) / (1.0f + falloff * coneDiameter);
		// march the cone
		t += coneDiameter * samplingFactor;
	}

	return vec4(marchedIrradiance.rgb, occlusion);
}


#endif //VOXEL_CONE_TRACING_GLSL_INCLUDE_GUARD