#version 460


#include "ch11_VoxelsSsbo.glsl"
#include "ch11_VoxelsTextures.glsl"

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/ViewProjectionBlock.glsl"


in vec2 ex_Uv;

uniform float u_VoxelSize;

uniform vec3 u_AabbMin;
uniform vec3 u_AabbMax;
uniform ivec2 u_FramebufferSize;
// Size of the image plane at a distance 1 from the camera origin
uniform vec2 u_ImagePlane_view;
uniform uint u_VoxelMode;
uniform int u_VoxelMipmapLevel;

const uniform vec4 u_MissColor = vec4(0.3, 0, 0, 1);

out vec4 out_Color;


bool isVoxelOccupied(ivec3 aVoxel)
{
    return getVoxelValue(aVoxel) == 1;
}


// TODO: there is an API design complication, because we might want
// internal values, such a tmin and tmax, to determine the entry / exit faces
vec2 intersectRayAabb(vec3 rayOrigin_world, vec3 rayDir_world,
                      vec3 aAabbMin, vec3 aAabbMax)
{
    // Time at slab 0 (the AABB origin)
    vec3 t_0 = (aAabbMin - rayOrigin_world) / rayDir_world;
    // Time at slab 1 (AABB opposite corner)
    vec3 t_1 = (aAabbMax - rayOrigin_world) / rayDir_world;

    vec3 tmin = min(t_0, t_1);
    vec3 tmax = max(t_0, t_1);

    float tIn = max(max(tmin.x, tmin.y), tmin.z);
    float tOut  = min(min(tmax.x, tmax.y), tmax.z);

    return vec2(tIn, tOut);
}


vec4 colorHitFace(bvec3 aMask)
{
    return vec4(vec3(dot(vec3(aMask), vec3(0.25, 0.5, 0.75))), 1);
}


float factorHitFace(bvec3 aMask)
{
    return dot(vec3(aMask), vec3(0.5, 0.75, 1));
}


vec4 fetchColor(ivec3 currentVoxel, bvec3 mask)
{
	switch(u_VoxelMode)
	{
		case CLIENT_VOXEL_MODE_OCCUPANCY:
			return colorHitFace(mask);
			break;

		case CLIENT_VOXEL_MODE_ALBEDO:
        {
            // Note: we could use texelFetch here too,
            // but this allows to validate the texture() code path
			vec4 color = vec4(
				vec3(texture(u_VoxelsAlbedoTexture, vec3(currentVoxel)/ub_GridDimension).rgb)
					* factorHitFace(mask)
					/ 255, 
				1);
			return correctGamma(color);
		}

		case CLIENT_VOXEL_MODE_NORMALS:
        {
            vec3 normal = vec3(texelFetch(u_VoxelsNormalsTexture, currentVoxel, 0).rgb);
			normal /= 255;  // RGBA8UI texture is not-normalized
            // At this point the normal is mapped to [0, 1]^3, which is what we need for display
            // (but they are not normalized)
			return vec4(normal, 1);

            //normal = normal * 2 - 1;
            //normal = (normalize(normal) + 1) / 2;
			//return vec4(normal, 1);
		}

		case CLIENT_VOXEL_MODE_IRRADIANCE:
        {
			vec4 color = vec4(
				vec3(texelFetch(u_VoxelsIrradianceTexture, currentVoxel, u_VoxelMipmapLevel).rgb) 
					    * factorHitFace(mask),
				     1);
			return correctGamma(color);
		}
	}
}


/// @param aTestOnTexture: If true, test voxel "occupancy" directly on the texture dictated by mode
/// otherwise, test it on the original voxels SSBBO (pure occupancy).
/// Note: testing on the texture directly is required when handling mipmap levels.
vec4 traverseVoxels(vec3 aRayEntry_aabb, vec3 aRayDir_aabb, 
					float voxelSize, uint gridDimension,
					inout bvec3 mask, bool aTestOnTexture)
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
	while(maxCw(currentVoxel) < gridDimension && minCw(currentVoxel) >= 0
		&& stp < maxSteps)
	{
		++stp;

		if (!aTestOnTexture && isVoxelOccupied(currentVoxel))
		{
			return fetchColor(currentVoxel, mask);
		}
		else if (aTestOnTexture && isTextureOccupied(currentVoxel, u_VoxelMipmapLevel, u_VoxelMode))
		{
			return fetchColor(currentVoxel, mask);
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
	}

	return u_MissColor;
}


void main(void)
{
    //
    // Define ray in world coordinates
    //

    vec2 fragPos_ndc = (gl_FragCoord.xy / u_FramebufferSize) * 2 - 1;
    // Important: the AABB is given in canonical world coordinates
    vec3 rayOrigin_world = getCameraPosition_world();
    // Note: we could cancel out the division by two from the multiplication in fragPos
    vec4 rayDir_view = vec4(fragPos_ndc * u_ImagePlane_view / 2, -1, 0);
    // Note: no need to normalize the direction
    vec3 rayDir_world = (ub_cameraToWorld * rayDir_view).xyz;
    

    //
    // Determine Ray / AABB intersection
    //

    // Time at slab 0 (the AABB origin)
    vec3 t_0 = (u_AabbMin - rayOrigin_world) / rayDir_world;
    // Time at slab 1 (AABB opposite corner)
    vec3 t_1 = (u_AabbMax - rayOrigin_world) / rayDir_world;

    vec3 tmin = min(t_0, t_1);
    vec3 tmax = max(t_0, t_1);

    float tIn = max(max(tmin.x, tmin.y), tmin.z);
    float tOut  = min(min(tmax.x, tmax.y), tmax.z);


    // In case we miss the AABB entirely, or do not hit any occupied voxel:
    out_Color = u_MissColor;

    // TODO: handle clipping planes
    if (tIn <= tOut && tOut >= 0)
    {
        // Intersection was found, clip the ray at camera position
        float t = max(0, tIn);
        
        // The mask indicate wether X, Y, or Z face was hit
        bvec3 mask;
        if(tmin.x > tmin.y && tmin.x > tmin.z)
        {
            mask = bvec3(true, false, false);
        }
        else if(tmin.y > tmin.z)
        {
            mask = bvec3(false,  true, false);
        }
        else
        {
            mask = bvec3(false, false,  true);
        }

        //#define DRAW_AABB
        #if defined(DRAW_AABB)
            out_Color = colorHitFace(mask);
        #else // DRAW_AABB 
			vec3 entry_world = rayOrigin_world + t * rayDir_world;
			vec3 entry_aabb = entry_world - u_AabbMin;

			int mipFactor = int(pow(2, u_VoxelMipmapLevel));
			float voxelSize = u_VoxelSize * mipFactor;
			uint gridDimension = ub_GridDimension / mipFactor;

			bool testOnTexture = (u_VoxelMode == CLIENT_VOXEL_MODE_IRRADIANCE);
			out_Color = traverseVoxels(entry_aabb, rayDir_world,
									   voxelSize, gridDimension,
									   mask, testOnTexture);
        #endif // DRAW_AABB
    }
}
