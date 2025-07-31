#if !defined(LIGHTSBLOCK_GLSL_INCLUDE_GUARD)
#define LIGHTSBLOCK_GLSL_INCLUDE_GUARD


#include "Constants.glsl"

struct LightColors
{
    vec4 diffuse;
    vec4 specular;
};


struct DirectionalLight
{
    vec4 direction;
    LightColors colors;
};


struct PointLight
{
    vec4 position;
    vec2 radius; 
    float wrapK;
    LightColors colors;
};


layout(std140, binding = 4) uniform LightsBlock
{
    // LightsDataUser
    uint ub_DirectionalCount;
    uint ub_PointCount;
    vec4 ub_AmbientColor;
    DirectionalLight ub_DirectionalLights[MAX_LIGHTS];
    PointLight ub_PointLights[MAX_LIGHTS];

    // Provide the light spatial information in view space too
    // WARNING: only used starting with ch11_voxels! Before everything was provided in view space
    vec4 ub_Directions_view[MAX_LIGHTS];
    vec4 ub_Points_view[MAX_LIGHTS];

    // LightsDataInternal
    //TODO: restore
    //uint ub_DirectionalLightShadowMapIndices[MAX_LIGHTS];
};


#endif //LIGHTSBLOCK_GLSL_INCLUDE_GUARD
