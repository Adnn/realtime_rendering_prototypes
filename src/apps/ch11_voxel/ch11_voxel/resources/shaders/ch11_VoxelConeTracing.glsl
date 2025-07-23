#if !defined(VOXEL_CONE_TRACING_GLSL_INCLUDE_GUARD)
#define VOXEL_CONE_TRACING_GLSL_INCLUDE_GUARD


#include "ch11_VoxelsSsbo.glsl"
#include "ch11_VoxelsTextures.glsl"

#include "shaders/Constants.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/PbrUtilities.glsl"


uniform float u_ConeMaxDistance = 10;
uniform float u_ConeOffsetAlongNormal = 1;
uniform float u_ConeOffsetAlongAxis = 1;
// To be used as aperture angle for diffuse cones
uniform float u_TanHalfAperture = M_PI / 6;
// Aperture angle for shadow cones
uniform float u_TanHalfShadow = 0.0174533f;
uniform float u_SpecularConeRoughnessFactor = 1;
uniform bool u_GridAlign;
uniform bool u_AnisotropicIrradianceMipmaps;


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


vec4 sampleAnisotropic(vec3 coord, float lod, vec3 weight, uvec3 face)
{
    // anisotropic volumes level
    float anisoLevel = max(lod - 1.0f, 0.0f);
    // directional sample
    vec4 anisoSample = weight.x * textureLod(u_VoxelsIrradianceAnisoMipmap[face.x], coord, anisoLevel)
                     + weight.y * textureLod(u_VoxelsIrradianceAnisoMipmap[face.y], coord, anisoLevel)
                     + weight.z * textureLod(u_VoxelsIrradianceAnisoMipmap[face.z], coord, anisoLevel);
    // linearly interpolate on base level
    if(lod < 1.0f)
    {
        vec4 baseSample = textureLod(u_VoxelsIrradianceTexture, coord, 0);
        anisoSample = mix(baseSample, anisoSample, max(0, lod));
    }

    return anisoSample;                    
}


vec4 sampleIsotropic(vec3 position_uvw, float aMipLevel)
{
    return textureLod(u_VoxelsIrradianceTexture, position_uvw, aMipLevel);
}


vec4 sampleIrradiance(vec3 direction_aabb, vec3 position_uvw, float mipLevel)
{
   if(u_AnisotropicIrradianceMipmaps)
   {
		uvec3 face = uvec3(
			direction_aabb.x >= 0 ? 0 : 1,
			direction_aabb.y >= 0 ? 2 : 3,
			direction_aabb.z >= 0 ? 4 : 5
		);
		vec3 weight = direction_aabb * direction_aabb;
		return sampleAnisotropic(position_uvw, mipLevel, weight, face);
	}
	else
	{
		return sampleIsotropic(position_uvw, mipLevel) ;
	}
}


/// @return The irradiance accumulated along the march in .rgb, the ambient occlusion in .a
vec4 traceCone(vec3 position_aabb, vec3 normal_aabb,
               vec3 coneAxis_aabb, float tanHalfAngle,
               float aVoxelSize)
{
    // A factor to implement the potential difference between d and d' in Crassin's paper.
    // This is beta in the explanation here: https://github.com/jose-villegas/VCTRenderer?tab=readme-ov-file#4-voxel-cone-tracing
    const float samplingFactor = 1;

    // Initial offset, to mitigate self-sampling.
    // The factor will be applied to the voxel size.
    const float offsetFactor = u_ConeOffsetAlongNormal;

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
    float t = u_ConeOffsetAlongAxis * aVoxelSize; // Another offset to limit self-sampling

    // ambient occlusion
    float occlusion = 0;
    vec4 marchedIrradiance = vec4(0);

    const ivec3 fragment_grid = ivec3(position_aabb / aVoxelSize);

    //#define ATTEMPT_REDUCE_CORNER_LIGHT_LEAKING
    #if defined(ATTEMPT_REDUCE_CORNER_LIGHT_LEAKING)
		// The idea here is to use the starting position sample alpha as pure occlusion:
        // only use it to increment the marched alpha, do not accumulate marched RGB irradiance.
        // Sadly, this also reduce intensity a lot where it looks good.
		vec3 samplePosition_aabb = startPosition_aabb + direction_aabb * t;
		vec3 position_uvw = samplePosition_aabb / (aVoxelSize * ub_GridDimension);
		vec4 irradianceSample = textureLod(u_VoxelsIrradianceTexture, position_uvw, 0);

		marchedIrradiance.a = irradianceSample.a;
		float coneDiameter = 2 * t * tanHalfAngle;
		t += max(aVoxelSize, coneDiameter) * samplingFactor;
		occlusion += (marchedIrradiance.a) / (1.0f + falloff * coneDiameter);
    #endif

    while(marchedIrradiance.a < 1.0f && t <= u_ConeMaxDistance)
    {
        float coneDiameter = 2 * t * tanHalfAngle;
        float mipLevel = log2(coneDiameter / aVoxelSize);

        vec3 samplePosition_aabb = startPosition_aabb + direction_aabb * t;

        const ivec3 sample_grid = ivec3(samplePosition_aabb / aVoxelSize);
        if(sample_grid == fragment_grid)
        {
            return vec4(1, 0, 1, 1);
        }

        // Johannes Finn add a 0.5 offset to the texture coordinates in:
        // Finn, Johannes. Evaluation of Performance and Image Quality for Voxel Cone Tracing
        // But it seems to me that it is not required to get the correct sampling position in the 3D texture
        vec3 position_uvw = samplePosition_aabb / (aVoxelSize * ub_GridDimension);

        vec4 irradianceSample = sampleIrradiance(direction_aabb, position_uvw, mipLevel);

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

        occlusion += ((1.0f - occlusion) * marchedIrradiance.a) / (1.0f + falloff * coneDiameter);
        // march the cone
        t += max(aVoxelSize, coneDiameter) * samplingFactor;
    }

    return vec4(marchedIrradiance.rgb, occlusion);
}


/// @return The irradiance accumulated along the march in .rgb, the ambient occlusion in .a
float traceShadow(vec3 position_aabb, vec3 normal_aabb,
                  vec3 coneAxis_aabb, float tanHalfAngle,
                  float aVoxelSize)
{
    // A factor to implement the potential difference between d and d' in Crassin's paper.
    // This is beta in the explanation here: https://github.com/jose-villegas/VCTRenderer?tab=readme-ov-file#4-voxel-cone-tracing
    const float samplingFactor = 1;

    // Initial offset, to mitigate self-sampling.
    // The factor will be applied to the voxel size.
    const float offsetFactor = u_ConeOffsetAlongNormal;

    // Sclaing factor
    float k = 1;

    // Note: Some implementation offset in the direction of the normal instead of the cone
    // e.g. https://github.com/jose-villegas/VCTRenderer/blob/9ae0dbe5bd60e85514e3e582bf23f2868c6b51fc/engine/assets/shaders/light_pass.frag#L147
    vec3 startPosition_aabb = position_aabb + normal_aabb * offsetFactor * aVoxelSize;

    // t : distance marched along the cone, in world unit
    float t = u_ConeOffsetAlongAxis * aVoxelSize; // Another offset to limit self-sampling

    // ambient occlusion
    float occupancy = 0;

    while(occupancy < 1.0f
          /* also breaks inside loop body if sampling outside the grid */)
    {
        float coneDiameter = 2 * t * tanHalfAngle;
        float mipLevel = log2(coneDiameter / aVoxelSize);

        vec3 samplePosition_aabb = startPosition_aabb + coneAxis_aabb * t;
        vec3 position_uvw = samplePosition_aabb / (aVoxelSize * ub_GridDimension);
        if(   any(lessThan(position_uvw, vec3(0)))
           || any(greaterThan(position_uvw, vec3(1))) )
        {
            break;
        }

        float occupancySample =
			sampleIrradiance(coneAxis_aabb, position_uvw, mipLevel).a
            * k
            ;
        occupancy += (1 - occupancy) * occupancySample;

        // march the cone
        t += coneDiameter * samplingFactor;
    }

    return (1 - occupancy);
}


/// @return The irradiance accumulated along the march in .rgb,
///         the ambient occlusion in .a
vec4 accumulateDiffuseIndirect(vec3 aPosition_aabb,
                               vec3 aNormal_world,
                               float aTanHalfAperture,
                               float aVoxelSize)
{
    // Prepare a tagent base (in which the diffuse cone directions are expressed)
    vec3 tangent_world, bitangent_world;
    revisedONB(aNormal_world, tangent_world, bitangent_world);
    mat3 tangentToWorld = mat3(tangent_world, bitangent_world, aNormal_world);

    const uint coneCount = 6;
    vec4 accumulatedIrradiance;
    for(uint i = 0; i != coneCount; ++i)
    {
        vec3 coneAxis_world = tangentToWorld * gDiffuseConeDirections[i];
        // Note: The AABB is aligned on world axis, so directions are matching
        accumulatedIrradiance +=
            traceCone(aPosition_aabb, aNormal_world,
                      coneAxis_world, aTanHalfAperture,
                      aVoxelSize)
                * gDiffuseConeWeights[i]
            ;
    }
    return accumulatedIrradiance;
}


/// @param aIncident_world, the incident direction, from eye to surface
///        (negation of the view vector)
vec4 accumulateSpecularIndirect(vec3 aPosition_aabb,
                                vec3 aNormal_world,
                                vec3 aIncident_world,
                                float aRoughness,
                                float aVoxelSize)
{
	vec3 reflectionDir_world= reflect(aIncident_world, aNormal_world);

    // Global user control of roughness
    aRoughness = clamp(aRoughness * u_SpecularConeRoughnessFactor, 0, 1);

	// Handle alpha
	float alpha = alphaFromRoughness(aRoughness);

	// Heuristic to map roughness to cone aperture
	const float maxHalfAngle = 1.262627f; // tan of this angle ~ Pi
	// TODO: should we use roughness or alpha?
	float halfAperture = maxHalfAngle * alpha;
	float tanHalfAperture = max(tan(halfAperture), 0.0174533f);

    // Debug point: make it very reflective
	//tanHalfAperture = 0.0174533f;

	return traceCone(aPosition_aabb,
					 aNormal_world,
					 reflectionDir_world,
					 tanHalfAperture,
					 aVoxelSize);

}


#endif //VOXEL_CONE_TRACING_GLSL_INCLUDE_GUARD
