#version 460


#include "ch11_VoxelsSsbo.glsl"
#include "ch11_VoxelsUtilities.glsl"

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/MaterialGenericBlock.glsl"
#include "shaders/PbrUtilities.glsl"
#include "shaders/ViewProjectionBlock.glsl"

in vec4 ex_Color;
in vec3 ex_Normal_world;
in vec3 ex_Position_world;
in vec2 ex_Uv01;

uniform uint u_MaterialIdx;

uniform uint u_MraoUvChannel;
uniform uint u_DiffuseUvChannel;
uniform sampler2D u_MraoTexture;
uniform sampler2D u_DiffuseTexture;

uniform sampler3D u_VoxelsAlbedoTexture;
uniform sampler3D u_VoxelsIrradianceTexture;

uniform float u_TanHalfAperture;

uniform float u_VoxelSize;
uniform vec3 u_AabbMin;
uniform bool u_GridAlign;

uniform uint u_ConeTraceMode;

out vec4 out_Color;

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
const vec3 diffuseConeDirections[] =
{
    vec3( 0.0f,		 0.0f,  1.0f),
    vec3( 0.0f,		  0.866025f,  0.5),
    vec3( 0.823639f,  0.267617f,  0.5),
    vec3( 0.509037f, -0.7006629f, 0.5),
    vec3(-0.50937f,  -0.7006629f, 0.5),
    vec3(-0.823639f,  0.267617f,  0.5)
};

const float diffuseConeWeights[] =
{
    M_PI / 4.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
};



/// @return The irradiance accumulated along the march in .rgb, the ambient occlusion in .a
vec4 traceCone(vec3 position_aabb, vec3 normal_aabb, vec3 coneAxis_aabb, float tanHalfAngle)
{
	// TODO: check reference implementation
	//const float maxDistance = 2;
	const float maxDistance = 10;

	// A factor to implement the potential difference between d and d' in Crassin's paper.
	// This is beta in the explanation here: https://github.com/jose-villegas/VCTRenderer?tab=readme-ov-file#4-voxel-cone-tracing
	const float samplingFactor = 1;

	// Initial offset, to mitigate self-sampling.
	// The factor will be applied to the voxel size.
	const float offsetFactor = 2;

	// TODO determine good range
	const float aoFalloff = 10;
	float falloff = 0.5f * aoFalloff / u_VoxelSize;

	vec3 direction_aabb = coneAxis_aabb;
	if (u_GridAlign)
	{
		position_aabb = (floor(position_aabb / u_VoxelSize) + vec3(0.5)) * u_VoxelSize;
	}

	// Note: Some implementation offset in the direction of the normal instead of the cone
	// e.g. https://github.com/jose-villegas/VCTRenderer/blob/9ae0dbe5bd60e85514e3e582bf23f2868c6b51fc/engine/assets/shaders/light_pass.frag#L147
	vec3 startPosition_aabb = position_aabb + normal_aabb * offsetFactor * u_VoxelSize; 

	// Distance marched along the cone, in world unit
	float t = 1.0 * u_VoxelSize; // Another offset to limit self-sampling

	// ambient occlusion
	float occlusion = 0;
	vec4 marchedIrradiance = vec4(0);

	const ivec3 fragment_grid = ivec3(position_aabb / u_VoxelSize);

	while(marchedIrradiance.a < 1.0f && t <= maxDistance)
	{
		float coneDiameter = 2 * t * tanHalfAngle;
		float mipLevel = log2(coneDiameter / u_VoxelSize);

		vec3 samplePosition_aabb = startPosition_aabb + direction_aabb * t;

		const ivec3 sample_grid = ivec3(samplePosition_aabb / u_VoxelSize);
		if(sample_grid == fragment_grid)
		{
			return vec4(1, 0, 1, 1);
		}

		vec3 position_uvw = samplePosition_aabb / (u_VoxelSize * ub_GridDimension);
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


// see: Building an Orthonormal Basis, Revisited (Pixar)
void revisedONB(vec3 n, out vec3 b1, out vec3 b2)
{
	if (n.z<0.0f)
	{
		const float a = 1.0f / (1.0f - n.z);
		const float b = n.x * n.y * a;
		b1 = vec3(1.0f - n.x * n.x * a, -b, n.x);
		b2 = vec3(b, n.y * n.y*a - 1.0f, -n.y);
	}
	else
	{
		const float a = 1.0f / (1.0f + n.z);
		const float b = -n.x * n.y * a;
		b1 = vec3(1.0f - n.x * n.x * a, b, -n.x);
		b2 = vec3(b, 1.0f - n.y * n.y * a, -n.y);
	}
}


void main(void)
{
    const uint gNoTextureChannel = uint(-1);

    //MaterialGeneric material = ub_MaterialGeneric[u_MaterialIdx];

    vec4 albedo = ex_Color;
    if(u_DiffuseUvChannel != gNoTextureChannel)
    {
        albedo *= texture(u_DiffuseTexture, ex_Uv01);
	}

    //
    // alpha testing for cutout
    //
    if (albedo.a < 0.5)
    {
        discard;
    }

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

		vec3 tangent, bitangent;
		revisedONB(normal_world, tangent, bitangent);
		mat3 tangentToWorld = mat3(tangent, bitangent, normal_world);

		const uint coneCount = 6;
		vec4 accumulatedIrradiance;
		for(uint i = 0; i != coneCount; ++i)
		{
			vec3 coneAxis_world = tangentToWorld * diffuseConeDirections[i];
			// Note: The AABB is aligned on world axis, so directions are matching
			accumulatedIrradiance += 
				traceCone(position_aabb, normal_world, coneAxis_world, u_TanHalfAperture)
					* diffuseConeWeights[i]
				;
		}

		// Specular
		vec4 specularIrradiance;
		{
			vec3 incident_world = ex_Position_world - getCameraPosition_world();
			vec3 reflectionDir_world= reflect(incident_world, normal_world);

			float metallic = 0.0;
			float roughness = 0.5;
			if(u_MraoUvChannel != gNoTextureChannel)
			{
				vec4 mrao = texture(u_MraoTexture, ex_Uv01);
				// glTF sponza channel order
				metallic = mrao.b;
				roughness = mrao.g;
			}

			// Handle alpha
			float alpha = alphaFromRoughness(roughness);

			// Heuristic to map roughness to cone aperture
			const float maxHalfAngle = 1.262627f; // tan of this angle ~ Pi
			// TODO: should we use roughness or alpha?
			float halfAperture = maxHalfAngle * roughness;
			float tanHalfAperture = max(tan(halfAperture), 0.0174533f);
			specularIrradiance = traceCone(position_aabb,
										   normal_world,
										   reflectionDir_world,
										   tanHalfAperture);

		}

		switch(u_ConeTraceMode)
		{
			case CLIENT_CONETRACE_AO:
				out_Color = vec4(vec3(1-accumulatedIrradiance.a), 1);
				break;
			case CLIENT_CONETRACE_DIFFUSE:
				out_Color = correctGamma(vec4(accumulatedIrradiance.rgb, 1));
				break;
			case CLIENT_CONETRACE_SPECULAR:
				out_Color = correctGamma(vec4(specularIrradiance.rgb, 1));
				break;
		}
	}
}
