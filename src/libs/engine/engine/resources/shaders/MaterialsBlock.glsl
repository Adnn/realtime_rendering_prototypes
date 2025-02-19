#if !defined(MATERIALSBLOCK_GLSL_INCLUDE_GUARD)
#define MATERIALSBLOCK_GLSL_INCLUDE_GUARD


#include "Constants.glsl"

struct Material
{
	float specularExponent;
    vec4 ambientColor;
    vec4 diffuseColor;
    vec4 specularColor;
};


layout(std140, binding = 2) uniform MaterialsBlock
{
    // LightsDataUser
    uint ub_Count;
    Material ub_Materials[MAX_MATERIALS];
};


#endif //MATERIALSBLOCK_GLSL_INCLUDE_GUARD
