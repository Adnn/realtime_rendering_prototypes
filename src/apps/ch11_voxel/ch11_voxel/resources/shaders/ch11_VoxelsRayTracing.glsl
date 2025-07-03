#if !defined(VOXEL_RAY_TRACING_GLSL_INCLUDE_GUARD)
#define VOXEL_RAY_TRACING_GLSL_INCLUDE_GUARD


#include "ch11_VoxelsSsbo.glsl"

#include "shaders/Helpers.glsl"


ivec3 traverseVoxels(vec3 aRayEntry_aabb, vec3 aRayDir_aabb, 
	   				 float voxelSize, uint gridDimension,
	   				 inout bvec3 mask, bool aTestOnTexture,
					 ivec3 aStopCoord, bool aSkipSelf)
{
	// see: "A Fast Voxel Traversal Algorithm for Ray Tracing", John Amanatides, Andrew Woo

	// 
	// Initialization phase
	//
	ivec3 currentVoxel = ivec3(aRayEntry_aabb / voxelSize);
	// Clamp to an actual voxel coordinate (rounding errors can introduce noise)
	currentVoxel = clamp(currentVoxel, ivec3(0), ivec3(gridDimension - 1));

	// Direction the grid is visited in each coordinate
	ivec3 step = ivec3(sign(aRayDir_aabb));
	currentVoxel += step;

	//#define INIT_SHADERTOY_FB39CA4
	#if defined INIT_SHADERTOY_FB39CA4
		// see: https://www.shadertoy.com/view/4dX3zl
		vec3 tDelta = abs( vec3(length(aRayDir_aabb) * voxelSize) / aRayDir_aabb );
		vec3 tMax = (step * (vec3(currentVoxel * voxelSize) - entry_aabb) + (step + 1) * 0.5 * voxelSize)
					* tDelta;
	#else // INIT_SHADERTOY_FB39CA4
		// Advancing by tDelta results in next position being N_1 = (t + tDelta) * rayDir
		// tDelta being 1/rayDir result in:
		// N_1 = (t * rayDir) + (1/rayDir * rayDir) = N_0 + rayDir/rayDir,
		// an increment of 1.
		// Note: the absolute value ensure each component of tDelta are positive 
		//       (even though the world direction could be negative)
		vec3 tDelta = abs(voxelSize / aRayDir_aabb);

		// Our grid is aligned to voxel corners: the boundaries' coordinates are
		// the (scaled) voxel coordinates.
		// Depending on rayDir's components sign, the next boundary is either:
		// * the current voxel coordinates (negative dir component)
		// * the next voxel coordinates (positive dir component).
		// Note: remap `step` from [-1, 1] to [0, 1]
		vec3 voxelBoundary_aabb = (currentVoxel + (step + 1.0) * 0.5) 
								  * voxelSize; 

		vec3 tMax = (voxelBoundary_aabb - aRayEntry_aabb) / aRayDir_aabb;
		// Equivalent to (related to the shadertoy formula):
		//vec3 tMax = (step * (voxelBoundary_aabb - entry_aabb)) * tDelta;
	#endif // INIT_SHADERTOY_FB39CA4

	//
	// Traversal phase
	//
	const uint maxSteps = (gridDimension * 3);
	// TODO: Having this max steps in place solve a potential hanging crash.
	// Understand why and better address it
	uint stp = 0;
	//while(maxCw(currentVoxel) < gridDimension && minCw(currentVoxel) >= 0

	bool stopSimple = currentVoxel == aStopCoord;
	bool stop = any(greaterThan((step * currentVoxel), (step * aStopCoord)));
	//bool stop = any(greaterThan((currentVoxel), (aStopCoord)));
	while(!stop
		  && stp < maxSteps)
	{
		++stp;

		// On 1st iteration, if skipself is true,just advance in the grid
		if(!aSkipSelf)
		{
			if (!aTestOnTexture && getVoxelValue(currentVoxel) == 1 )
			{
				return currentVoxel;
			}
			// TODO: make it more generic by handling all use cases
			//else if (aTestOnTexture && isTextureOccupied(currentVoxel, u_VoxelMipmapLevel))
			//{
			//	return fetchColor(currentVoxel, mask);
			//}
		}
		else
		{
			aSkipSelf = false;
		}

		if (tMax.x < tMax.y) 
		{
			if (tMax.x < tMax.z) 
			{
				tMax.x += tDelta.x;
				currentVoxel.x += step.x;
				mask = bvec3(true, false, false);
			}
			else
			{
				tMax.z += tDelta.z;
				currentVoxel.z += step.z;
				mask = bvec3(false, false, true);
			}
		}
		else 
		{
			if (tMax.y < tMax.z) 
			{
				tMax.y += tDelta.y;
				currentVoxel.y += step.y;
				mask = bvec3(false, true, false);
			}
			else 
			{
				tMax.z += tDelta.z;
				currentVoxel.z += step.z;
				mask = bvec3(false, false, true);
			}            
		}

		stop = any(greaterThan((step * currentVoxel), (step * aStopCoord)));
		stopSimple = (currentVoxel == aStopCoord);
	}

	return ivec3(-1);
}

#endif //VOXEL_RAY_TRACING_GLSL_INCLUDE_GUARD