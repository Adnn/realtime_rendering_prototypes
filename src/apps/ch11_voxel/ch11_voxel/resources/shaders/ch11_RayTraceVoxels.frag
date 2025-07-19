#version 460


#include "ch11_VoxelsRayTracing.glsl"
#include "ch11_VoxelsSsbo.glsl"
#include "ch11_VoxelsTextures.glsl"
#include "ch11_VoxelsUtilities.glsl"

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
uniform bool u_AnisotropicIrradianceMipmaps;

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
					* factorHitFace(mask), 
				1);
			return correctGamma(color);
		}

		case CLIENT_VOXEL_MODE_NORMALS:
        {
            vec3 normal = vec3(texelFetch(u_VoxelsNormalsTexture, currentVoxel, 0).rgb);
            // At this point the normal is mapped to [0, 1]^3, which is what we need for display
            // (but they are not normalized)
			return vec4(normal, 1);

            //normal = normal * 2 - 1;
            //normal = (normalize(normal) + 1) / 2;
			//return vec4(normal, 1);
		}

		case CLIENT_VOXEL_MODE_IRRADIANCE:
        {
            vec4 value;
            if(u_AnisotropicIrradianceMipmaps && (u_VoxelMipmapLevel > 0))
            {
				value = texelFetch(u_VoxelsIrradianceAnisoMipmap, currentVoxel, u_VoxelMipmapLevel - 1);
            }
            else
            {
				value = texelFetch(u_VoxelsIrradianceTexture, currentVoxel, u_VoxelMipmapLevel);
            }

			vec4 color = vec4(vec3(value.rgb) * factorHitFace(mask),
				              1);
			return correctGamma(color);
		}
	}
}


void main(void)
{
    //
    // Define ray in world coordinates
    //

    vec2 fragPos_ndc = (gl_FragCoord.xy / u_FramebufferSize) * 2 - 1;
    // Important: the AABB is given in canonical world coordinates
    vec3 rayOrigin_world = getCameraPosition_world();
    // Note: we could cancel out the division by two from the multiplication ;in fragPos
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

			const bool skipSelf = false;

			// Atm, all other modes sample in the occupancy SSBO (mipmap level is then irrelevant)
			VoxelOccupancy occupancy = (u_VoxelMode == CLIENT_VOXEL_MODE_IRRADIANCE) ?
				VoxelOccupancy(u_VoxelMode, u_VoxelMipmapLevel)
				: VoxelOccupancy(CLIENT_VOXEL_MODE_OCCUPANCY, 0);

            VoxelHit hit = VoxelHit(0, mask);
			ivec3 hit_grid = traverseVoxels(entry_aabb, rayDir_world,
			 			     			    voxelSize, gridDimension,
			 			     			    hit, occupancy,
			 			     			    getFullAabbBounds(), skipSelf);
			if(hit_grid == ivec3(-1))
            {
                out_Color = u_MissColor;
				gl_FragDepth = 1.0f;
            }
            else
            {
                out_Color = fetchColor(hit_grid, hit.mMask);

                // Compute frag depth
				vec3 hitPoint_aabb = (entry_aabb + hit.mT * rayDir_world);
				vec4 hitPoint_world = vec4(aabbToWorld(hitPoint_aabb, u_AabbMin), 1);
				vec4 hitPoint_clip = ub_viewingProjection * hitPoint_world;
                float depth_ndc = hitPoint_clip.z / hitPoint_clip.w;
                // Remap from NDC [-1, 1] to window coordinates [0, 1]
				gl_FragDepth = (depth_ndc + 1) / 2;
			}
        #endif // DRAW_AABB
    }
}
