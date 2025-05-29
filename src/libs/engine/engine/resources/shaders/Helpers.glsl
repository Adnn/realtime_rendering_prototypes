#if !defined(HELPERS_GLSL_INCLUDE_GUARD)
#define HELPERS_GLSL_INCLUDE_GUARD


#include "Constants.glsl"


float dotPlus(vec3 a, vec3 b)
{
    return max(0.f, dot(a, b));
}


float maxCw(vec3 v)
{
    return max(max(v.x, v.y), v.z);
}


float maxCw(vec4 v)
{
    //see: https://stackoverflow.com/a/77071476
    vec2 pairs = max(v.xy, v.zw);
    return max(pairs.x, pairs.y);
}


float minCw(vec3 v)
{
    return min(min(v.x, v.y), v.z);
}


float minCw(vec4 v)
{
    //see: https://stackoverflow.com/a/77071476
    vec2 pairs = min(v.xy, v.zw);
    return min(pairs.x, pairs.y);
}


// Remaps a vector from symmetric domain [-amplitude, amplitude]^3 to [0, 1]^3.
// Notably useful to display unit direction vectors as colors.
vec3 mapToRgb(vec3 aInput, float aAmplitude)
{
    return (aInput + vec3(aAmplitude)) / (2 * aAmplitude);
}

// Remaps a unit vector from [-1, 1]^3 to [0, 1]^3.
vec3 mapToRgb(vec3 aInput)
{
    return mapToRgb(aInput, 1);
}


vec3 highlightAberrations(vec3 aColor)
{
    const float limit = 1.0/255;
    if (aColor.r < limit && aColor.g < limit && aColor.b < limit)
    {
        return vec3(1.0, 0.0, 1.0);
    }
    else if(any(isnan(aColor)))
    {
        return vec3(0.0, 1.0, 0.0);
    }
    else if(any(isinf(aColor)))
    {
        return vec3(1.0, 0.0, 1.0);
    }
    else
    {
        return aColor;
    }
}


/// @param aWorldRay direction in world space, expected in the usual right-handed world basis.
vec3 worldToCubemap(vec3 aWorldRay)
{
    // The cubemap basis is left-handed
    // see: https://www.khronos.org/opengl/wiki/Cubemap_Texture
    return vec3(aWorldRay.xy, -aWorldRay.z);
}


vec2 worldToEquirectangular(vec3 aWorldRay)
{
    // For the conversion procedure, see: rtr 4th p407
    // (the book does metion +z is up, but does not define the complete basis.
    // I suppose it is the physic basis from: 
    // https://en.wikipedia.org/wiki/Spherical_coordinate_system,
    // so x becomes y, y becomes z, z becomes x).
    vec3 sampleDir = normalize(aWorldRay);

    // This formula show the middle of the equirectangle with default camera looking down -Z (in world).
    // The value is mirrored on the range [0, 1]:
    // an increasing azimuth (counterclockwise) has to lead to a decreasing u to avoid mirroring.
    float u = 1 - atan(sampleDir.x, sampleDir.z) / (2 * M_PI);
    // Which give the same result as:
    //float u = atan(-sampleDir.x, sampleDir.z) / (2 * M_PI);

    // Polar angle increase in the opposite direction compared to v coordinate
    float v = 1 - acos(sampleDir.y) / M_PI;

    return vec2(u, v);
}


#endif //include guard
