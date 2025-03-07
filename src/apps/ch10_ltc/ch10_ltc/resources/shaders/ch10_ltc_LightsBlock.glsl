#if !defined(LIGHTSBLOCK_GLSL_INCLUDE_GUARD)
#define LIGHTSBLOCK_GLSL_INCLUDE_GUARD


#include "shaders/Constants.glsl"


struct LightColors
{
    vec4 diffuse;
    vec4 specular;
};


struct CardLight
{
    float height;
    bool doubleSided;
    vec2 rectPosition;
    vec2 rectDimensions;
    LightColors colors;
};


vec3[4] getPolygon(CardLight aCard)
{
    vec3 result[4];

    vec3 origin = vec3(aCard.rectPosition.x, aCard.height, aCard.rectPosition.y);
    vec3 w = vec3(aCard.rectDimensions.x, 0, 0);
    vec3 h = vec3(0, 0, aCard.rectDimensions.y);

    result[0] = origin;
    result[1] = origin + h;
    result[2] = origin + w + h;
    result[3] = origin + w;

    return result;
}


layout(std140, binding = 4) uniform LightsBlock
{
    uint ub_PlanarCount;
    vec4 ub_AmbientColor;
    CardLight ub_PlanarLights[MAX_LIGHTS];
};


#endif //LIGHTSBLOCK_GLSL_INCLUDE_GUARD
