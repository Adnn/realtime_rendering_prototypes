#if !defined(VOXEL_RAY_TRACING_GLSL_INCLUDE_GUARD)
#define VOXEL_RAY_TRACING_GLSL_INCLUDE_GUARD


#include "ch11_VoxelsSsbo.glsl"
#include "ch11_VoxelsTextures.glsl"

#include "shaders/Helpers.glsl"


struct GridBounds
{
    ivec3 mMin;
    ivec3 mMax;
};


GridBounds getFullAabbBounds()
{
    return GridBounds(ivec3(0), ivec3(ub_GridDimension - 1));
}


struct VoxelOccupancy
{
    uint mMode;
    int mTextureLevel;
};


VoxelOccupancy makeOccupancy()
{
    return VoxelOccupancy(CLIENT_VOXEL_MODE_OCCUPANCY, 0);
}


bool testOnTexture(VoxelOccupancy aOccupancy)
{
    return aOccupancy.mMode != CLIENT_VOXEL_MODE_OCCUPANCY;
}


struct VoxelHit
{
    float mT;
    bvec3 mMask; // whether the hit occurent on X, Y, or Z axis
};


/// @param voxelSize is the size of a voxel in the basis of the aabb
/// @param aOccupancyMethod: Control if the test for voxel "occupancy" is on the texture 
///           corresponding to mMode or on the occupancy SSBO.
///        Note: testing on the texture itself is required when handling mipmap levels.
ivec3 traverseVoxels(vec3 aRayEntry_aabb, vec3 aRayDir_aabb, 
                     float voxelSize, uint gridDimension,
                     // Even though we never read from aHit, needs to be inout to maintain
                     // the initial value if it is not modified
                     inout VoxelHit aHit,
                     VoxelOccupancy aOccupancyMethod,
                     GridBounds aBounds,
                     bool aSkipSelf)
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
        vec3 tMax = (step * (vec3(currentVoxel * voxelSize) - aRayEntry_aabb) + (step + 1) * 0.5 * voxelSize)
                    * tDelta;
    #else // INIT_SHADERTOY_FB39CA4
        // Advancing by tDelta results in next position being N_1 = (t + tDelta) * rayDir
        // tDelta being voxelSize/rayDir result in:
        // N_1 = (t * rayDir) + (voxelSize/rayDir * rayDir) = N_0 + voxelSize,
        // an increment of 1 voxel (on each component).
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

        // The quantity of advancement along ray-dir to reach the voxel boundary (for each component)
        vec3 tMax = (voxelBoundary_aabb - aRayEntry_aabb) / aRayDir_aabb;
        // Equivalent to (related to the shadertoy formula):
        //vec3 tMax = (step * (voxelBoundary_aabb - aRayEntry_aabb)) * tDelta;
    #endif // INIT_SHADERTOY_FB39CA4

    //
    // Traversal phase
    //
    const uint maxSteps = (gridDimension * 3);
    // TODO: Having this max steps in place solve a potential hanging crash.
    // Understand why and better address it
    uint stp = 0;

    // Note: we could test only once per component based on the step sign.
    //bool stop = any(greaterThan((step * currentVoxel), (step * aStopCoord)));
    while(   all(greaterThanEqual(currentVoxel, aBounds.mMin))
          && all(lessThanEqual(currentVoxel, aBounds.mMax))
          && stp < maxSteps)
    {
        ++stp;

        // On 1st iteration, if skipself is true,just advance in the grid
        if(!aSkipSelf)
        {
            if (   (!testOnTexture(aOccupancyMethod) 
                    && getVoxelValue(currentVoxel) == 1) 
                || ( testOnTexture(aOccupancyMethod) 
                    && isTextureOccupied(currentVoxel, aOccupancyMethod.mTextureLevel, aOccupancyMethod.mMode)))
            {
                return currentVoxel;
            }
        }
        else
        {
            aSkipSelf = false;
        }

        if (tMax.x < tMax.y) 
        {
            if (tMax.x < tMax.z) 
            {
                aHit.mT = tMax.x;
                aHit.mMask = bvec3(true, false, false);
                tMax.x += tDelta.x;
                currentVoxel.x += step.x;
            }
            else
            {
                aHit.mT = tMax.z;
                aHit.mMask = bvec3(false, false, true);
                tMax.z += tDelta.z;
                currentVoxel.z += step.z;
            }
        }
        else 
        {
            if (tMax.y < tMax.z) 
            {
                aHit.mT = tMax.y;
                aHit.mMask = bvec3(false, true, false);
                tMax.y += tDelta.y;
                currentVoxel.y += step.y;
            }
            else 
            {
                aHit.mT = tMax.z;
                aHit.mMask = bvec3(false, false, true);
                tMax.z += tDelta.z;
                currentVoxel.z += step.z;
            }            
        }
    }

    return ivec3(-1);
}

#endif //VOXEL_RAY_TRACING_GLSL_INCLUDE_GUARD