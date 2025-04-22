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
    bool clipHorizon;
    vec2 rectPosition;
    vec2 rectDimensions;
    float rotationX;
    float rotationZ;
    LightColors colors;
};


vec3[4] getPolygon(CardLight aCard)
{
    vec3 result[4];

    float rx = aCard.rotationX;
    float rz = aCard.rotationZ;

    vec3 origin = vec3(aCard.rectPosition.x, aCard.height, aCard.rectPosition.y);
    // TODO: in production, precompute the polygon vertices instead of 
    // transforming each frame in each fragment
    vec3 w = aCard.rectDimensions.x * vec3(cos(rz), sin(rz), 0);
    vec3 h = aCard.rectDimensions.y * vec3(sin(rz) * sin(rx), -cos(rz) * sin(rx), cos(rx));

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
