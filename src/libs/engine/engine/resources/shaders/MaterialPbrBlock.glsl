#if !defined(MATERIALPBRBLOCK_GLSL_INCLUDE_GUARD)
#define MATERIALPBRBLOCK_GLSL_INCLUDE_GUARD


#include "Constants.glsl"


struct MaterialPbr
{
    vec4 ambientColor; // This is a convenience to control some constant ambient contribution
    vec4 baseColor;
    vec2 metallicRoughness;
};


layout(std140, binding = 2) uniform MaterialPbrBlock
{
    uint ub_Count;
    MaterialPbr ub_MaterialPbr[MAX_MATERIALS];
};


#endif //MATERIALPBRBLOCK_GLSL_INCLUDE_GUARD
