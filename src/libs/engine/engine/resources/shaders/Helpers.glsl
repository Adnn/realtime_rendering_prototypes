#if !defined(HELPERS_GLSL_INCLUDE_GUARD)
#define HELPERS_GLSL_INCLUDE_GUARD


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


/// @param aWorldDirection direction in world space, expected in the usual right-handed world basis.
vec3 worldToCubemap(vec3 aWorldDirection)
{
    // The cubemap basis is left-handed
    // see: https://www.khronos.org/opengl/wiki/Cubemap_Texture
    return vec3(aWorldDirection.xy, -aWorldDirection.z);
}


#endif //include guard
