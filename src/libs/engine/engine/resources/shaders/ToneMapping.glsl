#if !defined(TONEMAPPING_GLSL_INCLUDE_GUARD)
#define TONEMAPPING_GLSL_INCLUDE_GUARD


vec3 tonemapReinhard(vec3 aColor)
{
	return aColor / (aColor + 1);
}

//
// ACES
// see: https://github.com/TheRealMJP/BakingLab/blob/v1.0/BakingLab/ACES.hlsl
// note: GLSL matrices are column major, so the literal value is transposed
//

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
    1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
);

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 tonemapAces(vec3 aColor)
{
    aColor = ACESInputMat * aColor;
    aColor = RRTAndODTFit(aColor);
    aColor = ACESOutputMat * aColor;
    return clamp(aColor, 0, 1);
}

vec3 tonemapAces_approx(vec3 v)
{
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp(
        (v*(a*v+b)) / (v*(c*v+d)+e),
        0, 1);
}


#endif //include guard
