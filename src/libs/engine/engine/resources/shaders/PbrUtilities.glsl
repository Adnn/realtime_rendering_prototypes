#if !defined(PBRUTILITIES_GLSL_INCLUDE_GUARD)
#define PBRUTILITIES_GLSL_INCLUDE_GUARD


const vec3 gF0_dielec = vec3(0.04);
const vec3 gF90= vec3(1.0);


/// @brief PBR parameters tailored for BRDFs formulas.
/// It is intended as a convenient parameter to PBR light computations.
struct PbrParameters
{
    vec3 diffuseColor;
    vec3 f0;
    vec3 f90;
    float alpha; // from roughness
};


float heaviside(float aValue)
{
    // Note: I do not think we can implement it as step(0., aValue)
    // since it would return 1 when aValue is 0.
    return float(aValue > 0.);
}


/// @brief Schlick approximation of the Fresnel reflectance equation.
vec3 schlickFresnelReflectance(float aNormalDotLight_plus, vec3 F0, vec3 F90)
{
    return F0 + (F90 - F0) * pow(1 - aNormalDotLight_plus, 5);
}


/// @brief Schlick approximation of the Fresnel reflectance equation.
vec3 schlickFresnelReflectance(vec3 aNormal, vec3 aLightDir, vec3 F0, vec3 F90)
{
    return schlickFresnelReflectance(dotPlus(aNormal, aLightDir), F0, F90);
}


/// @brief Comput alpha from roughness parameter
float alphaFromRoughness(float aRoughness)
{
    // Disney PBR square the user provided roughness value (r) to obtain alpha.
    // hard to say if the metallic-roughness map already contains it squared, which would save instructions
    // (but would change the precision distribution)
    //
    // Also, from "Real Shading in Unreal Engine 4", some other massaging of roughness are possible.
    // For examplen to reduce "hotness" alpha = ((roughness + 1) / 2) ^ 2
    return aRoughness * aRoughness;
}


//////////
// BRDFs
//////////

// Important: All BRDFs are given already mutiplied by Pi
// (because the correct formulas usually have a Pi in the denominator).


////
// Diffuse BRDF
////

/// @brief Lambertian BRDF, weighted by (1-F) as to represent the energy trade-off.
/// @attention Already mutiplied by Pi
/// @see rtr 4th p351
vec3 diffuseBrdf_weightedLambertian(vec3 aFresnelReflectance, vec3 aDiffuseColor)
{
    return (1 - aFresnelReflectance) * aDiffuseColor;
}


//
// Trowbridge-Reitz / GGX microfacet model
//

/// @attention Returned mutiplied by Pi, , so the overall GGX specular BRDF is mutiplied by Pi
/// @param aAlphaSq cannot be null
float Distribution_GGX(float nDotH, float aAlphaSq)
{
    // Note: I do not know how to handle alpha == 0:
    // the function becomes an asymptote where result is 0 but tends to +inf as nDotH tends to 1.

    float d = 1 + nDotH * nDotH * (aAlphaSq - 1);
    // TODO is heaviside useful here?
    return (heaviside(nDotH) * aAlphaSq) / (d * d);
}


/// @param aDot is either vDotL or lDotN
float Lambda_GGX(float aDot, float aAlphaSq)
{
    float dotSq = aDot * aDot;
    // rtr 4th p339 (9.37)
    float aSq = dotSq / (aAlphaSq * (1 - dotSq));
    // rtr 4th p341 (9.42)
    return (-1. + sqrt(1. + (1. / aSq))) / 2.;
}


/// @brief Smith height-correlated masking-shadowing function
/// @see rtr 4th p335
float G2_GGX(float nDotL, float nDotV, float aAlphaSq)
{
    // Our equivalent to heaviside (not sure how relevant it is)
    if ((nDotV == 0) || (nDotL == 0))
    {
        return 0.f;
    }
    else
    {
        float d = 1 + Lambda_GGX(nDotV, aAlphaSq) + Lambda_GGX(nDotL, aAlphaSq);
        return 1.f / d;
    }
}


// Note: Visibility is how we call the the compound term: G2 / (4 |n.l| |n.v|)
// TODO: Find the correct name, "visibility" might be an abuse.
//       Some texts seem to use "visibility" for the masking-shadowing function (G2).

/// @brief The explicit longform of the visibility (please use the a simplification/approximation for prod)
/// It is mainly intended to test the G2_GGX.
float Visibility_GGX_longform(float nDotL, float nDotV, float aAlphaSq)
{
    // Guard against division by zero (redundant with heaviside in G2_GGX)
    if ((nDotV == 0) || (nDotL == 0))
    {
        return 0;
    }
    else
    {
        return G2_GGX(nDotL, nDotV, aAlphaSq) / (4 * nDotL * nDotV);
    }
}


/// @note This is the simplified (yet I assume exact) form of the height-correlated Smith G2 for GGX
/// see rtr 4th p341
float Visibility_GGX(float nDotL, float nDotV, float aAlphaSq)
{
    float uo = nDotV;
    float ui = nDotL;
    float l = uo * sqrt(aAlphaSq + ui * (ui - aAlphaSq * ui));
    float r = ui * sqrt(aAlphaSq + uo * (uo - aAlphaSq * uo));
    // I suppose there is a risk of division by zero
    // because I sometime get dark outlines if not making the test
    float d = l + r;
    if (d > 0)
    {
        return 0.5 / d;
    }
    else
    {
        return 0;
    }
}

// Hammon-Karis approximation of Trowbridge-Reitz/GGX visibility
// see: rtr 4th p342
float Visibility_GGX_approx(float nDotL, float nDotV, float aAlpha)
{
    // The approximation suffer from the same problem of division by zero
    // so we test the upper bound
    if ((nDotL + nDotV) > 0)
    {
        return 0.5 / mix(2 * nDotL * nDotV, nDotL + nDotV, aAlpha);
    }
    else
    {
        return 0;
    }
}

//#define APPROXIMATE_G2_GGX
#define SIMPLIFIED_VISIBILITY_GGX
//#define EXPLICIT_G2_GGX

// Important: All BRDFs are given already mutiplied by Pi
// (because the correct formulas usually have a Pi in the denominator)
vec3 specularBrdf_GGX(vec3 aFresnelReflectance, 
                      float nDotH, float nDotL, float nDotV,
                      float aAlpha)
{
    float alphaSquared = aAlpha * aAlpha;
    float D = Distribution_GGX(nDotH, alphaSquared);
#ifdef APPROXIMATE_G2_GGX
    float V = Visibility_GGX_approx(nDotL, nDotV, aAlpha);
#elif defined(SIMPLIFIED_VISIBILITY_GGX)
    float V = Visibility_GGX(nDotL, nDotV, alphaSquared);
#elif defined(EXPLICIT_G2_GGX)
    float V = Visibility_GGX_longform(nDotL, nDotV, alphaSquared);
#endif
    return aFresnelReflectance * V * D;
}
#endif //PBRUTILITIES_GLSL_INCLUDE_GUARD


////
// Beckmann & Blinn-Phong models
////

/// @param aAlpha_beckmann Alpha value according to Bekcmann model.
///        **Attention**: must not be zero, has to be strictly positive.
float alphaBeckmannToPhong(float aAlpha_beckmann)
{
    return 2 * pow(aAlpha_beckmann, -2) - 2;
}


/// @param nDotH_plus dotPlus of N and H.
/// @attention returned already mutiplied by Pi
/// @note see rtr 4th p340
float Distribution_BlinnPhong(float nDotH_plus, float aAlpha_phong)
{
    // Note: The formula in rtr 4th p340 use the raw dot product, but we use dotPlus.
    // Rationale: GLSL pow() is undefined for x < 0., and this should be equivalent since
    // the heaviside operator in the formula means any negative nDotH should return 0,
    // which is also the case when nDotH is 0.
    // Note: we probably do not need heaviside, since 0^y is 0 (for any non-nul y).
    return /*heaviside(nDotH_plus) * */((aAlpha_phong + 2) / 2) * pow(nDotH_plus, aAlpha_phong);
}


/// @note The exact Lambda is known but considered too expensive for real-time.
///       see rtr 4th p339
/// @param aDot nDotL or nDotV, unclamped and signed.
/// @param aAlpha_beckmann should be strictly positive, **cannot** be null.
float Lambda_Beckmann_approximate(float aDot, float aAlpha_beckmann)
{
    float a = aDot / (aAlpha_beckmann * sqrt(1 - (aDot * aDot)));
    if (a < 1.6)
    {
        return (1 - 1.259 * a + 0.396 * a * a)
                / (3.535 * a + 2.181 * a * a);
    }
    else
    {
        return 0;
    }
}


/// @param nDotL is the **unclamped** signed dot product ("raw" dot product).
/// @param nDotV is the **unclamped** signed dot product ("raw" dot product).
float Visibility_Beckmann(float nDotL, float nDotV, float aAlpha_beckmann)
{
    float lambda_v = Lambda_Beckmann_approximate(nDotV, aAlpha_beckmann);
    float lambda_l = Lambda_Beckmann_approximate(nDotL, aAlpha_beckmann);
    float G2 = 1 / (1 + lambda_v + lambda_l);

    return G2 / (4 * abs(nDotL) * abs(nDotV));
}


/// @brief Overload taking the unit vectors directly, so there is no client-error
/// regarding dot products post-op.
float Visibility_Beckmann(vec3 n, vec3 l, vec3 v, float aAlpha_beckmann)
{
    return Visibility_Beckmann(dot(n, l), dot(n, v), aAlpha_beckmann);
}


/// @important aAlpha_beckmann cannot be zero, as it create a lot of numerical issues
/// (for the moment, this function ensures it is not, which has a runtime cost)
vec3 specularBrdf_BlinnPhong(vec3 aFresnelReflectance, 
                             float nDotH_plus, float nDotL_raw, float nDotV_raw,
                             float aAlpha_beckmann)
{
    // It is mandatory that alpha_beckmann is not null 
    // otherwise the conversion returns inf
    // (which is theoretically correct, but numerically problematic)
    // and the visibility returns nan.
    float alpha_b_safe = max(0.0001, aAlpha_beckmann);
    float D = Distribution_BlinnPhong(nDotH_plus, alphaBeckmannToPhong(alpha_b_safe));
    // rtr 4th p340 suggests using the Beckmann Lambda (thus, visibility).
    float V = Visibility_Beckmann(nDotL_raw, nDotV_raw, alpha_b_safe);
    return aFresnelReflectance * V * D;
}

