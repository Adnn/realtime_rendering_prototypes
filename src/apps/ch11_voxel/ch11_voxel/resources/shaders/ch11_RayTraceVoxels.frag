#version 460


#include "shaders/Helpers.glsl"
#include "shaders/ViewProjectionBlock.glsl"


in vec2 ex_Uv;

uniform uint u_GridSide;
uniform float u_VoxelSide;

layout(std430, binding = 10) buffer VoxelsSsbo
{
  uint ub_Voxels[];
};

out vec4 out_Color;


uniform vec3 u_AabbMin;
uniform vec3 u_AabbMax;
uniform ivec2 u_FramebufferSize;
// Size of the image plane at a distance 1 from the camera origin
uniform vec2 u_ImagePlane_view;


uint isVoxelOccupied(ivec3 aVoxel)
{
	const uint voxelPerUint = 4; // Cpp uint8_t per GLSL uint
	uint xStride = u_GridSide / voxelPerUint;
	uint yStride = xStride * u_GridSide;
	unsigned int idx = xStride * aVoxel.x 
					   + yStride * aVoxel.y 
					   + uint(floor(aVoxel.z / voxelPerUint))
					   ;

	return (ub_Voxels[idx] >> ((aVoxel.z % voxelPerUint) * 8)) & 0xFF;
}


void main(void)
{
	vec2 fragPos_ndc = (gl_FragCoord.xy / u_FramebufferSize) * 2 - 1;
	// Important: the AABB is given in canonical world coordinates
	vec3 rayOrigin_world = getCameraPosition_world();
	// Note: we could cancel out the division by two from the multiplication in fragPos
	vec4 rayDir_view = vec4(fragPos_ndc * u_ImagePlane_view / 2, -1, 0);
	vec3 rayDir_world = (ub_cameraToWorld * rayDir_view).xyz;
	//rayDir_world = normalize(rayDir_world);
	
	// Time at slab 0 (the AABB origin)
	vec3 t_0 = (u_AabbMin - rayOrigin_world) / rayDir_world;
	// Time at slab 1 (AABB opposite corner)
	vec3 t_1 = (u_AabbMax - rayOrigin_world) / rayDir_world;

	vec3 tmin = min(t_0, t_1);
	vec3 tmax = max(t_0, t_1);

	float tIn = max(max(tmin.x, tmin.y), tmin.z);
	float tOut  = min(min(tmax.x, tmax.y), tmax.z);

	// TODO: handle clipping planes
	if (tIn <= tOut && tOut >= 0)
	{
		// Intersection found
		float t = max(0, tIn);
		
		//#define DRAW_AABB
		#if defined(DRAW_AABB)
			if(tmin.x > tmin.y && tmin.x > tmin.z)
			{
				out_Color = vec4(vec3(0.75), 1);
			}
			else if(tmin.y > tmin.z)
			{
				out_Color = vec4(vec3(0.5), 1);
			}
			else
			{
				out_Color = vec4(vec3(0.25), 1);
			}
			return;
		#endif	

		vec3 entry_world = rayOrigin_world + t * rayDir_world;
		vec3 entry_aabb = entry_world - u_AabbMin;
		ivec3 currentVoxel = ivec3(floor(entry_aabb / u_VoxelSide));
		// Clamp to an actual voxel coordinate (rounding errors can introduce noise)
		currentVoxel = clamp(currentVoxel, ivec3(0), ivec3(u_GridSide - 1));

		// Direction the grid is visited in each coordinate
		ivec3 step = ivec3(sign(rayDir_world));

		//#define INIT_SHADERTOY_FB39CA4
		#if defined INIT_SHADERTOY_FB39CA4
			// see: https://www.shadertoy.com/view/4dX3zl
			vec3 tDelta = abs( vec3(length(rayDir_world) * u_VoxelSide) / rayDir_world );
			vec3 tMax = (step * (vec3(currentVoxel) - entry_aabb) + (step * 0.5) + 0.5)
						* tDelta;
		#else
			// advancing by tDelta results in next position being N_1 = (t + tDelta) * rayDir
			// tDelta being 1/rayDir result in:
			// N_1 = (t * rayDir) + (1/rayDir * rayDir) = N_0 + rayDir/rayDir, an increment of 1.
			// Note: the absolute value ensure each component of tDelta are positive 
			//       (even though the world direction could be negative)
			vec3 tDelta = abs(1 / rayDir_world);

			// Our grid is aligned to voxel corners: the boundaries' coordinates are
			// the voxel coordinates.
			// Depending on rayDir's components sign, the next boundary is either:
			// * the current voxel coordinates (negative dir component)
			// * the next voxel coordinates (positive dir component).
			// Note: remap `step` from [-1, 1] to [0, 1]
			vec3 voxelBoundary = currentVoxel + (step + 1.0) * 0.5; 

			vec3 tMax = (voxelBoundary - entry_aabb) / rayDir_world;
			// Equivalent to (related to the shadertoy formula):
			//vec3 tMax = (step * (voxelBoundary - entry_aabb)) * tDelta;
		#endif

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

		// TODO: replace with AABB exitance condition
		#define MAX_RAY_STEPS 32
		//for (int i = 0; i < MAX_RAY_STEPS; i++) 

		bool found = false;
		//while(minCw(tMax) <= tOut)
		while(maxCw(currentVoxel) < u_GridSide && minCw(currentVoxel) >= 0)
		{
			if (isVoxelOccupied(currentVoxel) == 1)
			{
				// TODO get rid of this boolean
				found = true;
				break;
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
		if(found)
		{
			out_Color = vec4(vec3(dot(vec3(mask), vec3(0.25, 0.5, 0.75))), 1);
		}
		else
		{
			out_Color = vec4(0.3, 0, 0, 1);
		}
	}
	else
	{
		out_Color = vec4(0.3, 0, 0, 1);
	}

}
