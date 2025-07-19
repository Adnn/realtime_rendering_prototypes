#if !defined(LIGHTVIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD)
#define LIGHTVIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD


#include "Constants.glsl"

struct VertexProjection
{
	vec3 position[MAX_SHADOW_MAPS];
};


layout(std140, binding = 5) uniform LightViewProjectionBlock
{
    uint ub_LightViewProjectionCount;
    mat4 ub_LightViewProjections[MAX_SHADOW_MAPS];
};


#endif // include guard