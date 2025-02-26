#if !defined(LIGHTUTILITIES_GLSL_INCLUDE_GUARD)
#define LIGHTUTILITIES_GLSL_INCLUDE_GUARD


#include "LightsBlock.glsl"


struct LightContributions
{
	vec3 diffuse;
	vec3 specular;
};


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


// see: Karis, Brian, "Real Shading in Unreal Engine 4," p15
/// @return A modified light direction
vec3 representativePoint_sphere(vec3 aFragmentPosition,
                                vec3 aLightCenter,
                                vec3 aReflectionDir,
                                float aSphereRadius)
{
    // shaded point to light-sphere center
    vec3 L = aLightCenter - aFragmentPosition;

    // Note: here, Kaaris use the negation of what is presented in rtr 4th fig 10.10
    // We negate Kaaris formulation to get the correct result with our conventions.
    // From the sphere center to the closest point on the reflection ray:
    vec3 centerToRay = dot(L, aReflectionDir) * aReflectionDir - L;
    // Note: the paper use single bars around center to ray, we take it to mean the norm
    vec3 closestPoint = L + centerToRay * clamp(aSphereRadius / length(centerToRay), 0., 1.);
    // Note: here, the paper use double bars, but result is a vector. We take it to mean normalization.
    return normalize(closestPoint);

    // Note: below is rtr 4th fig 10.10 formalization:
    //vec3 pcr = dot(L, aReflectionDir) * aReflectionDir - L;
    //vec3 pcs = L + pcr * min(1, aSphereRadius / length(pcr));
    //return normalize(pcs);
}



#endif //LIGHTUTILITIES_GLSL_INCLUDE_GUARD
