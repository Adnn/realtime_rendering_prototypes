#if !defined(LIGHTUTILITIES_GLSL_INCLUDE_GUARD)
#define LIGHTUTILITIES_GLSL_INCLUDE_GUARD


#include "LightsBlock.glsl"


// Compute inverse-square light attenuation multiplied by a windowing function
// see: rtr 4th p113 (5.14) 
float attenuatePoint(PointLight aLight, float aRadius)
{
    return 
        pow(aLight.radius[0] / max(aRadius, aLight.radius[0]),
            2)
        *
        pow(max(0, 
                (1 - pow(aRadius / aLight.radius[1], 4))),
            2);
}


#endif //LIGHTUTILITIES_GLSL_INCLUDE_GUARD
