#version 460
// Seems to allow `layout()` qualifiers on image as function parameters
// from: https://github.com/jose-villegas/VCTRenderer/blob/7ae9788f25ef46ab4f9ece2e8cbcf158934ec3a0/engine/assets/shaders/voxelization.frag#L2
// for issue, see: https://github.com/KhronosGroup/GLSL/issues/57
#extension GL_ARB_shader_image_load_store : require


#include "ch11_VoxelsSsbo.glsl"

#include "shaders/Helpers.glsl"
#include "shaders/MaterialGenericBlock.glsl"


/// See: Crassin, Cyril, and Simon Green. “Octree-Based Sparse Voxelization Using the GPU Hardware Rasterizer.” In OpenGL Insights, edited by Patrick Cozzi and Christophe Riccio, 303–20. A K Peters/CRC Press, 2012.
/// Listing 22.2

/// \brief Convert a 32-bit unsigned integer storing an RGBA8 to a **denormalized** vec4
/// R in leftmost bits, A in rightmost bits (see the masks).
vec4 convRGBA8ToVec4(uint val)
{
    return vec4(
		float((val & 0x000000FF)),			// R
		float((val & 0x0000FF00) >> 8U),	// G
		float((val & 0x00FF0000) >> 16U),	// B
		float((val & 0xFF000000) >> 24U));	// A
}

/// \brief Convert a **denormalized** vec4 to 
/// a 32-bit unsigned integer storing an RGBA8.
uint convVec4ToRGBA8(vec4 val)
{
    return
	  (uint(val.a) & 0x000000FF) << 24U | 
      (uint(val.b) & 0x000000FF) << 16U | 
      (uint(val.g) & 0x000000FF) <<  8U | 
      (uint(val.r) & 0x000000FF);
}

void imageAtomicRGBA8Avg(layout(r32ui) volatile coherent restrict uimage3D grid,
						 ivec3 coords,
						 vec4 value)
{
	// Denormalize value that is expected to represent an usual normalized vector
    value.rgb *= 255.0;
    uint newVal = convVec4ToRGBA8(value);
    uint prevStoredVal = 0;
    uint curStoredVal;
    uint numIterations = 0;
	const uint gIterationLimit = 256;

	// Attempt to atomically swap with newVal,
	// until the stored value (before swap) does match prevStoredVal.
    while((curStoredVal = imageAtomicCompSwap(grid, coords, prevStoredVal, newVal)) 
            != prevStoredVal
            && numIterations < gIterationLimit)
    {
		// The swap failed: update newVal
        prevStoredVal = curStoredVal;
        vec4 rval = convRGBA8ToVec4(curStoredVal);
		// Compute cumulative average, with the number of samples stored in alpha
        rval.rgb = (rval.rgb * rval.a); // Current average times the number of samples it covers
        vec4 curValF = rval + value;    // Add the current sample (value.rgb) to the multiplied average (rval.rbg)
										// Add 1 (value.a) to the samples count (rval.a)
        curValF.rgb /= curValF.a;       // Divide by the updated sample count
		// Set the uint value to swap with next try
        newVal = convVec4ToRGBA8(curValF);

        ++numIterations;
    }
}


in vec3 ex_Position_grid;
in vec4 ex_Color;
in vec3 ex_Normal_world;
in vec2 ex_Uv01;

uniform sampler2D u_DiffuseTexture;
uniform sampler2D u_NormalTexture;

uniform uint u_DiffuseUvChannel;
uniform uint u_NormalUvChannel;

uniform bool u_ConservativeDepthRange;
uniform bool u_AverageSamples = true;
uniform bool u_AverageNormalByAxis;

uniform uint u_MaterialIdx;

// * coherent: memory accesses are coherant with similar access from other shader invocations
// * volatile (seems to imply coherent): the memory can be read or written during 
// * restrict: there is no access aliasing (no other image variable access the same data)
//   shader exection by some other source than the executing shader.
layout(r32ui) uniform coherent volatile restrict uimage3D u_AlbedoImage;
layout(r32ui) uniform coherent volatile restrict uimage3D u_NormalsImage;


void recordVoxel(ivec3 aGridCoordinate, vec4 unmultipliedAlbedo, vec3 aNormal)
{
	markOccupiedAtomic(aGridCoordinate);

	if(u_AverageNormalByAxis)
	{
		aNormal = abs(aNormal);
	}

	vec3 remappedNormal = mapToUnit(aNormal);

	if(u_AverageSamples)
	{
		//#define PREMULTIPLY_ALPHA
		#if defined(PREMULTIPLY_ALPHA)
			// For correct color averaging, the alpha must be premultiplied to linear space colors (already linear)
			// see: https://github.com/jose-villegas/VCTRenderer/blob/7ae9788f25ef46ab4f9ece2e8cbcf158934ec3a0/engine/assets/shaders/voxelization.frag#L111-L112
			vec4 albedo = vec4(
				unmultipliedAlbedo.rgb * unmultipliedAlbedo.a,
				unmultipliedAlbedo.a); // Actually, weighting the cumulative average avoids over-darkening in zones with low-alpha
		#else
			// Sadly, the above solution has major drawback:
			// * an alpha < 1 will be truncated to 0 by convVec4ToRGBA8()
			//   This does result in incorrect weightin, and potential divide by zero
			// * setting alpha to 1 (to count as 1 whole sample) with premultiplying
			//   will overdarken the texels where a lot of low alpha pixels are present
			// Note: this darkeking might be mitigated during the cone-trace
			// if we forward some notion of coverage related to those low-alpha
			vec4 albedo = vec4(
				unmultipliedAlbedo.rgb,
				1); // Count as 1 sample in the average, see cumulative average implementation
		#endif
		imageAtomicRGBA8Avg(u_AlbedoImage, aGridCoordinate, albedo);

		// Each normal count as 1 sample in the average, so alpha is set to 1.
		// See cumulative average implementation
		vec4 normalSample = vec4(remappedNormal, 1);
		imageAtomicRGBA8Avg(u_NormalsImage, aGridCoordinate, normalSample);
	}
	else
	{
		imageAtomicExchange(
			u_AlbedoImage, 
			aGridCoordinate, 
			convVec4ToRGBA8(unmultipliedAlbedo * 255));
		imageAtomicExchange(
			u_NormalsImage, 
			aGridCoordinate, 
			convVec4ToRGBA8(vec4(remappedNormal, 0) * 255));
	}
}


void main(void)
{
    const uint gNoTextureChannel = uint(-1);

    // Albedo values are already in linear space
    vec4 albedo = ex_Color;
    if(u_DiffuseUvChannel != gNoTextureChannel)
    {
        albedo *= texture(u_DiffuseTexture, ex_Uv01);
	}

	// Apply the material diffuse color as factor to the fragment albedo.
    MaterialGeneric material = ub_MaterialGeneric[u_MaterialIdx];
	albedo *= material.diffuseColor;

	// Alpha testing
	if (albedo.a < 0.5)
	{
		discard;
	}

	vec3 normal = normalize(ex_Normal_world);

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

		for (int z = int(zMin); z <= int(zMax); ++z)
		{
			// Truncate the floats toward zero (equivalent to floor() on Z+)
			recordVoxel(ivec3(ivec2(ex_Position_grid.xy), z), albedo, normal);
		}
	}
	else
	{
		// Truncates toward zero, as needed (equivalent to floor in Z+)
		ivec3 voxel = ivec3(ex_Position_grid);
		recordVoxel(voxel, albedo, normal);
	}
}
