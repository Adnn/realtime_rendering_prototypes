#if !defined(MATERIALPBRBLOCK_GLSL_INCLUDE_GUARD)
#define MATERIALPBRBLOCK_GLSL_INCLUDE_GUARD


#include "Constants.glsl"


struct MaterialGeneric
{
    vec4 ambientColor;
    vec4 diffuseColor;
    vec4 specularColor;
    uint diffuseTextureIndex;
    uint diffuseUvChannel;
    uint normalTextureIndex;
    uint normalUvChannel;
    uint mraoTextureIndex;
    uint mraoUvChannel;
    float metallicFactor;
    float roughnessFactor;
    float specularExponent;
};


layout(std140, binding = 2) uniform MaterialGenericBlock
{
    uint ub_Count;
    MaterialGeneric ub_MaterialGeneric[MAX_MATERIALS];
};


#endif //MATERIALPBRBLOCK_GLSL_INCLUDE_GUARD
