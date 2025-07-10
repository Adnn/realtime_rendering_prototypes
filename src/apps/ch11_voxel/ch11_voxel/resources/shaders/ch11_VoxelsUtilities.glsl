#if !defined(VOXEL_UTILITIES_GLSL_INCLUDE_GUARD)
#define VOXEL_UTILITIES_GLSL_INCLUDE_GUARD


vec3 voxelCenterToWorld(ivec3 aGridPosition, float aVoxelSize, vec3 aGridMin_world)
{
	// Offset by 0.5 to position at the voxel center
    vec3 position_grid = vec3(aGridPosition) + 0.5;
    return position_grid * aVoxelSize + aGridMin_world;
}


vec3 voxelToWorld(vec3 aFractionalPosition_grid, float aVoxelSize, vec3 aGridMin_world)
{
    return aFractionalPosition_grid * aVoxelSize + aGridMin_world;
}


ivec3 worldToVoxel(vec3 aPosition_world, float aVoxelSize, vec3 aGridMin_world)
{
    return ivec3((aPosition_world - aGridMin_world) / aVoxelSize);
}


vec3 worldToAabb(vec3 aPosition_world, vec3 aGridMin_world)
{
    return (aPosition_world - aGridMin_world);
}

vec3 aabbToWorld(vec3 aPosition_aabb, vec3 aGridMin_world)
{
    return (aPosition_aabb + aGridMin_world);
}



#endif //VOXEL_UTILITIES_GLSL_INCLUDE_GUARD