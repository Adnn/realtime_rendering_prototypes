//
// See: Real Shading in Unreal Engine 4, Brian Karis
//

#if !defined(HELPERS_IBL_GLSL_INCLUDE_GUARD)
#define HELPERS_IBL_GLSL_INCLUDE_GUARD


#include "Constants.glsl"
#include "Helpers.glsl"
#include "PbrUtilities.glsl"


//////
// see: https://learnopengl.com/PBR/IBL/Specular-IBL
//////

float radicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}


/// @brief Provide the Hammersley Sequence 
/// A random low-discrepency sequence usefull for importance sampling
/// in quasi Monte Carlo integration method.
vec2 hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), radicalInverse_VdC(i));
}  


//////
//
//////


/// @param Xi (Chi) a random uniform sample on [0, 1].
/// @param N The surface normal in a coordinate system that will be used for the result.
/// @return A half-vector (H) <b>in the same space as N</b>
///         that allows importance sampling of the GGX specular lobe.
vec3 importanceSampleGGX(vec2 Xi, float aAlphaSquared, vec3 N)
{
    float Phi = 2 * M_PI * Xi.x;
    // Inverse transform sampling (by cumulative distribution function inversion)
    // see: https://chatgpt.com/share/faad0830-d3b4-4c91-9abc-49a0cdce4dc6
    float CosTheta = sqrt((1 - Xi.y) / (1 + (aAlphaSquared - 1) * Xi.y));
    float SinTheta = sqrt(1 - CosTheta * CosTheta);
    vec3 H;
    // H is the cartesian coordinates of half vector in tangent space.
    // (This is the spherical-to-cartesion from angles relative to the implicit normal and tangent)
    H.x = SinTheta * cos( Phi );
    H.y = SinTheta * sin( Phi );
    H.z = CosTheta;
    // Compute the coordinates of the tagent space basis vectors in the space of N
    // (the resulting tangent space is right-handed)
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 TangentX = normalize(cross(UpVector, N));
    vec3 TangentY = cross(N, TangentX);
    // TBN to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}


/// @see "Moving Frostbite to Physically Based Rendering", listing 18.
/// @param Xi (Chi) a random uniform sample on [0, 1].
/// @param N The surface normal in a coordinate system that will be used for the result.
/// @return A light direction (L) <b>in the same space as N</b>
///         that allows importance sampling of the diffuse cosine lobe.
vec3 importanceSampleCosDir(vec2 Xi, vec3 N)
{
    // Local referencial
    vec3 upVector = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0) ;
    vec3 tangentX = normalize(cross(upVector , N));
    vec3 tangentY = cross(N , tangentX);

    float r = sqrt(Xi.x);
    // The same than importantSampleGGX (just using the other dimension)
    float phi = 2 * M_PI * Xi.y;

    vec3 L = vec3(r * cos (phi) , r * sin (phi) , sqrt(max(0.0f, 1.0f - Xi.x)));
    return normalize(tangentX * L.y + tangentY * L.x + N * L.z);

    //NdotL = dot (L ,N) ;
    //pdf = NdotL * FB_INV
}


/// @brief Compute the LOD approximating the cone matching distance between samples,
/// for the special case of the isotropic assumption.
/// see: rtr 4th p419
/// see: [GPU Gems 3 chapter 20](https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling)
/// @param aLodUniformTerm a term constant for a given texture size and sample count, that is computed by computeLodUniformTerm()
/// @note This is hardcoding the GGX NDF when getting D to compute the PDF.
/// @warning This is hardcoding the anisotropic assumption used in split-sum approximation, that N == V == R.
float computeLodIsotropic_GGX(float aLodUniformTerm, float NoH, float aAlphaSquared, 
                              uint aNumSamples/* TODO: remove when the LearnOpenGL approach is removed*/)
{
    // From "Real Shading in Unreal Engine 4": pdf = D * NoH / (4 * VoH)
    // From GPU Gems 3 chapter 20:
    // lod = max(0, 1/2 log2(w*h/N) - 1/2 log2(p(u,v) d(u)))

    // Note: NoH == VoH because of the isotropic assumption (see above that N == V == R).
    // Note: The article was using dual parabolloid mapping (where distortion d(u) gets large)
    //       For cubemap, distortion is low, let's assume it is constant to 1 atm.
    // Note: Distribution_GGX returns the NDF multiplied by Pi (not sure we have to divide though)
    float D = Distribution_GGX(NoH, aAlphaSquared) / M_PI;
    float lod = max(0, aLodUniformTerm - (log2(D / 4) / 2));

#if 0 
    // Alternative implementation from https://chetanjags.wordpress.com/2015/08/26/image-based-lighting/
    // which gives visually very similar/identical results (I think it is equivalent up to a constant factor)
    float pdf = (D /** NdotH*/ / (4.0 /** HdotV*/)) + 0.0001; 
    float resolution = 2048.0; // resolution of source cubemap (per face)
    float saTexel  = 4.0 * M_PI / (6.0 * resolution * resolution);
    float saSample = 1.0 / (float(aNumSamples) * pdf + 0.0001);
    lod = 0.5 * log2(saSample / saTexel); 
#endif

    return lod;
}


float computeLodGeneric_GGX(float aLodUniformTerm, float NoH, float VoH, float aAlphaSquared)
{
    // From "Real Shading in Unreal Engine 4": pdf = D * NoH / (4 * VoH)
    // From GPU Gems 3 chapter 20:
    // lod = max(0, 1/2 log2(w*h/N) - 1/2 log2(p(u,v) d(u)))

    // Note: Distribution_GGX returns the NDF multiplied by Pi (not sure we have to divide though)
    float D = Distribution_GGX(NoH, aAlphaSquared) / M_PI;
    return max(0, aLodUniformTerm - (log2((D * NoH) / (4 * VoH)) / 2));
}


float computeLod_Cosine(float aLodUniformTerm, float NoL)
{
    // From "Porting Frostbite to Physically Based Rendering": pdf = NoL / Pi 
    // From GPU Gems 3 chapter 20:
    // lod = max(0, 1/2 log2(w*h/N) - 1/2 log2(p(u,v) d(u)))

    float lod = max(0, aLodUniformTerm - (log2(NoL / M_PI) / 2));
    return lod;
}


float computeLodUniformTerm(ivec2 aEnvSize, uint aNumSamples)
{
    return log2(aEnvSize.x * aEnvSize.y / aNumSamples) / 2.0;
}


// Use the environment mipmaps when prefiltering, which immensely helps with aliasing
#define PREFILTER_LEVERAGE_LOD

/// @brief Specular contribution of Image Based Lighting from a cube environment map.
/// 
/// Computed via quasi Monte Carlo importance sampling of the environment map.
vec3 specularIBL(vec3 aSpecularColor, float aAlphaSquared, vec3 N, vec3 V, samplerCube aEnvMap)
{
    //const uint NumSamples = 1024;
    const uint NumSamples = 256;

    vec3 specularLighting = vec3(0);

    // We assume that if a fragment made it there, it's normal N have to make it visible from V.
    // Yet, it is possible that the dot product is zero (even negative with normal mapping).
    // Initially, we returned zero if (NoV <= 0),
    // but this lead to black pixels on some borders where strong Fresnel was expected instead.
    // Now, clamp to a small positive value instead:
    float NoV = max(0.000001, dot(N, V));

#if defined(PREFILTER_LEVERAGE_LOD)
    // Does not depend on the sample
    float lodUniformTerm = computeLodUniformTerm(textureSize(aEnvMap, 0), NumSamples);
#endif

    for(uint i = 0; i < NumSamples; i++)
    {
        vec2 Xi = hammersley(i, NumSamples);
        vec3 H = importanceSampleGGX(Xi, aAlphaSquared, N);
        vec3 L = reflect(-V, H);
        float NoL = dotPlus(N, L);
        float NoH = dotPlus(N, H);
        float VoH = dotPlus(V, H);
        if(NoL > 0)
        {
#if defined(PREFILTER_LEVERAGE_LOD)
            // I think NoH and VoH have to be > 0 when getting there
            float lod = computeLodGeneric_GGX(lodUniformTerm, NoH, VoH, aAlphaSquared);
#else
            float lod = 0.;
#endif
            vec3 sampleColor = textureLod(aEnvMap, vec3(L.xy, -L.z), lod).rgb;

            float G = G2_GGX(NoL, NoV, aAlphaSquared);
            vec3 F = schlickFresnelReflectance(VoH, aSpecularColor, vec3(1., 1., 1.));
            // Incident light = sampleColor * NoL
            // Microfacet specular = D*G*F / (4*NoL*NoV)
            // pdf = D * NoH / (4 * VoH)
            specularLighting += sampleColor * F * G * VoH / (NoH * NoV);
        }
    }
    return specularLighting / NumSamples;
}


/// @param R the \b normalized reflection direction in world basis
/// (i.e. the direciton that would be sampled in the filtered map)
vec3 prefilterEnvMapSpecular(float aAlphaSquared, vec3 R, samplerCube aEnvMap)
{
    const uint NumSamples = 1024;

    // To use any lobe, this approach imposes n = v = r
    // see: rtr 4th p421
    // This is an isotropic assumption, making the lobe radially symmetric.
    vec3 N = R;
    vec3 V = R;
    
#if defined(PREFILTER_LEVERAGE_LOD)
    float lodUniformTerm = computeLodUniformTerm(textureSize(aEnvMap, 0), NumSamples);
#endif

    // The accumulator
    vec3 prefilteredColor = vec3(0);
    float totalWeight = 0.f;
    for(uint i = 0; i < NumSamples; i++)
    {
        vec2 Xi = hammersley(i, NumSamples);
        vec3 H = importanceSampleGGX(Xi, aAlphaSquared, N);
        vec3 L = reflect(-V, H);
        float nDotL = dotPlus(N, L);
        if(nDotL > 0)
        {
            vec3 sampleDir = worldToCubemap(L);
            
#if defined(PREFILTER_LEVERAGE_LOD)
            vec3 sampled;
            // Improtant: Distribution_GGX does not handle alpha == 0 (the theoretical result is +inf)
            // Anyway, we should probably always sample the mipmap level matching pixel coverage 
            // (only one sample direction should be possible when alpha == 0)
            if(aAlphaSquared != 0.)
            {
                // TODO: clarify why both following sources diverge.
                // As far as I understand, Karis (UE4 paper) compute the pre-filtered radiance
                // by importance sampling the radiance (mutiplied by the cos(theta_light))
                // but rtr 4th p421 seems to indicate that it should be multiplied by the distribution D.
                // Doing this multiplication seems to diverge a lot from specularIbl(),
                // so we avoid it.
                //float NoH = dot(N, H); // factorize if kept
                //float D = Distribution_GGX(NoH, aAlphaSquared) / M_PI;
                // Additional notes: The reasoning might be that rtr gives the equation as integrals
                // but the importance-sampled integrator additionally has the PDF in the denominator 
                // (which puts D in the denominator, cancelling out).
                // I suspect this is equivalent up to a constant factor.
                float lod = computeLodIsotropic_GGX(lodUniformTerm, dot(N, H), aAlphaSquared, NumSamples);
                sampled = textureLod(aEnvMap, sampleDir, lod).rgb;
            }
            else
            {
                // The sample direction should be unique, with L == N == V == R.
                // i.e. we can leverage normal LODs calculation
                // Also, we should only ever take 1 sample...
                sampled = texture(aEnvMap, sampleDir).rgb;
            }
#else
            // Limiting light intensity can reduce aliasing due to hug contributions, like the sun
            //vec3 sampled = min(vec3(100.),
            //                   textureLod(aEnvMap, vec3(L.xy, -L.z), 0).rgb);
            vec3 sampled = textureLod(aEnvMap, sampleDir, 0).rgb;
#endif

            prefilteredColor += sampled * nDotL;
            totalWeight += nDotL; // theoretically, the weight should always be 1
                                  // but Karis states "weighting by cos achieves better results" p5.
        }
    }

    return prefilteredColor / totalWeight;
}


/// @brief Integrate the environment map convolved with a cosine lobe center on the normal N.
/// @param R the \b normalized normal direction in world basis
/// (i.e. the direciton that would be sampled in the filtered map)
vec3 prefilterEnvMapDiffuse(vec3 N, samplerCube aEnvMap)
{
    const uint NumSamples = 1024;

#if defined(PREFILTER_LEVERAGE_LOD)
    // Does not depend on the sample
    float lodUniformTerm = computeLodUniformTerm(textureSize(aEnvMap, 0), NumSamples);
#endif

    // The accumulator
    vec3 prefilteredColor = vec3(0);
    for(uint i = 0; i < NumSamples; i++)
    {
        vec2 Xi = hammersley(i, NumSamples);
        vec3 L = importanceSampleCosDir(Xi, N);
        float nDotL = dot(N, L);
        if(nDotL > 0)
        {
            vec3 sampleDir = worldToCubemap(L);
            
            // see: mftpbr p67
            // pdf = (n.l) / Pi, for a sample distribution following a Cosine lobe
            // We want to integrate Env(l) * (n . l) over the hemisphere centered on n.
            // Env(l) * (n.l) / pdf = Pi * Env(l), Pi can be factored out of the sum.
#if defined(PREFILTER_LEVERAGE_LOD)
            float lod = computeLod_Cosine(lodUniformTerm, nDotL);
            vec3 sampled = textureLod(aEnvMap, sampleDir, lod).rgb;
#else
            vec3 sampled = textureLod(aEnvMap, sampleDir, 0).rgb;
#endif

            prefilteredColor += sampled;
        }
    }

    // Note: Later, if applying a diffuse brdf, the Pi might vanish into it (see mftpbr p67)
    // For e.g., look at prefilterEnvMapDiffuse_LambertianFresnel()
    return M_PI * prefilteredColor / NumSamples;
}


/// @brief Integrate the diffuse reflectance equation with a BRDF
/// based on the Lambertian model for a flat mirror.
/// @note: using e.q. (9.62), for flat mirrors instead of microfacet BRDF
/// because (9.63) depends upon view direction (to get H).
vec3 prefilterEnvMapDiffuse_LambertianFresnel(vec3 N, vec3 F0, samplerCube aEnvMap)
{
    const uint NumSamples = 1024;

#if defined(PREFILTER_LEVERAGE_LOD)
    // Does not depend on the sample
    float lodUniformTerm = computeLodUniformTerm(textureSize(aEnvMap, 0), NumSamples);
#endif

    // The accumulator
    vec3 prefilteredColor = vec3(0);
    for(uint i = 0; i < NumSamples; i++)
    {
        vec2 Xi = hammersley(i, NumSamples);
        vec3 L = importanceSampleCosDir(Xi, N);
        float nDotL = dot(N, L);
        if(nDotL > 0)
        {
            vec3 sampleDir = worldToCubemap(L);
            
            // We want to integrate the diffuse part of the reflectance equation:
            //    f_{diff}(l) * L_i(l) * (n . l), over the hemisphere centered on n.
            // rtr eq (9.62):
            // f_{diff}(l) = (1 - F(n, l)) * rho_{ss}/Pi
            // L_i(l) radiance is obtained by sampling the environment
            // see: mftpbr p67
            // pdf = (n.l) / Pi, for a sample distribution following a Cosine lobe
            // f_{diff}(l) * L_i(l) * (n.l) / pdf = (1 - F(n, l)) * rho_{ss} * Env(l),
            // rho_{ss} is constant an is factored out of the integral (actually applied by calling code)
#if defined(PREFILTER_LEVERAGE_LOD)
            float lod = computeLod_Cosine(lodUniformTerm, nDotL);
            vec3 sampled = textureLod(aEnvMap, sampleDir, lod).rgb;
#else
            vec3 sampled = textureLod(aEnvMap, sampleDir, 0).rgb;
#endif

            vec3 F = schlickFresnelReflectance(nDotL, gF0_dielec, vec3(1., 1., 1.));
            prefilteredColor += max(vec3(0.), (1 - F)) * sampled;
        }
    }

    return prefilteredColor / NumSamples;
}


/// @param NoV is theretically the value clamped to [0..1], 
///        but it should not be equal to zero, otherwise leading to div by 0.
vec2 integrateBRDF(float aAlphaSquared, float NoV)
{
    const uint NumSamples = 1024;

    // View vector in tangent space
    vec3 V;
    V.x = sqrt(1.0f - NoV * NoV); // sin
    V.y = 0;
    V.z = NoV; // cos
    
    const vec3 normal_tbn = vec3(0, 0, 1);

    float A = 0; // Scale accumulator
    float B = 0; // Bias accumulator
    for( uint i = 0; i < NumSamples; i++ )
    {
        vec2 Xi = hammersley(i, NumSamples);
        // H in tangent space
        vec3 H = importanceSampleGGX(Xi, aAlphaSquared, normal_tbn);
        vec3 L = 2 * dot( V, H ) * H - V;
        float NoL = max(0, L.z); // normal being (0, 0, 1);
        float NoH = max(0, H.z);
        float VoH = dotPlus(V, H);
        if(NoL > 0)
        {
            // since theta is the angle between normal and light, cos(theta) == NoL
            // here we aim to integrate : X = (f * cos(theta)) / pdf == F * (G * VoH) / (NoH * NoV)
            // (this can be verified by expanding the terms and simplifying)
            // setting G_vis = (G * VoH) / (NoH * NoV), we have X == F * G_vis
            // subsituting F = F0 + (1 - F0)(1 - VoH)^5
            // we get X = F0 * (G_vis * (1 - (1 - VoH)^5)) + G_vis * (1 - VoH)^5
            // (which maps to equation 8 in the paper)
            // Here we can accumulate the scale (G_vis * (1 - (1 - VoH)^5)) in A
            // and the bias (G_vis * (1 - VoH)^5) in B
            float G = G2_GGX(NoL, NoV, aAlphaSquared);
            float G_Vis = (G * VoH) / (NoH * NoV);
            float Fc = pow(1 - VoH, 5);
            A += (1 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return vec2(A, B) / NumSamples;
}

// Apply the correction discussed in
// "Moving Frostbite to Physically Based Rendering 3.0", p10
// TODO: I suppose more places should be skewed by this define.
//#define OFF_SPECULAR_PEAK

// Taken from "Moving Frostbite to Physically Based Rendering 3.0"
// Listing 22 p69
// Original comment: We have a better approximation of the off specular peak
// but due to the other approximations we found this one performs better .
// N is the normal direction
// R is the mirror vector
// This approximation works fine for G smith correlated and uncorrelated
vec3 getSpecularDominantDir (vec3 N , vec3 R , float roughness)
{
    float smoothness = clamp(1 - roughness, 0, 1);
    float lerpFactor = smoothness * (sqrt (smoothness) + roughness);
    // The result is not normalized as we fetch in a cubemap
    return mix(N , R , lerpFactor);
 }


/// @brief Split-sum approximation of the specular contribution from IBL
/// Acts as an approximate and much faster specularIbl()
vec3 approximateSpecularIbl(vec3 aSpecularColor,
                            vec3 aReflection,
                            vec3 aNormal,
                            float NoV,
                            float aRoughness,
                            samplerCube aFilteredRadiance,
                            sampler2D aIntegratedBrdf)
{
    // TODO replace with an uniform
    const int lodCount = textureQueryLevels(aFilteredRadiance);

    float lod = aRoughness * float(lodCount);
    vec3 cubemapSampleDir = 
#if defined(OFF_SPECULAR_PEAK)
        worldToCubemap(getSpecularDominantDir(aNormal, aReflection, aRoughness));
#else
        worldToCubemap(aReflection);
#endif
    vec3 filteredRadiance = textureLod(aFilteredRadiance, cubemapSampleDir, lod).rgb;

    vec2 brdf = texture(aIntegratedBrdf, vec2(NoV, aRoughness)).rg;
    
    // The F90 term was found in mftpbr eq. (58)
    return filteredRadiance * (aSpecularColor * brdf.r + /*F90 * */brdf.g);
}



/// This version attempts to also sample from the envmap if it has a LOD higher
/// than the filtered radiance (with hope it will give better sampling)
/// Yet it makes it much too sharp, so this is considered a failure.
vec3 approximateSpecularIbl_alterLod(vec3 aSpecularColor,
                            vec3 aReflection,
                            vec3 aReflectionBase,
                            float NoV,
                            float aRoughness,
                            samplerCube aFilteredRadiance,
                            samplerCube aEnvMap,
                            sampler2D aIntegratedBrdf)
{
    // TODO replace with an uniform
    const int lodCount = textureQueryLevels(aFilteredRadiance);

    float lod_hardware = textureQueryLod(aEnvMap, worldToCubemap(aReflectionBase)).y;
    vec3 envSample = textureLod(aEnvMap, worldToCubemap(aReflectionBase), lod_hardware).rgb;

    float lod = aRoughness * float(lodCount);
    vec3 cubemapSampleDir = worldToCubemap(aReflection);
    //max(lod, lod_hardware) was making it too blurry
    vec3 filteredSample = textureLod(aFilteredRadiance, cubemapSampleDir, lod).rgb;

    vec3 filteredRadiance = filteredSample;
    if(lod_hardware > lod)
    {
        float diff = (lod_hardware - lod) / lodCount;
        filteredRadiance = mix(filteredSample, envSample, diff * (1. - aRoughness));
    }

    vec2 brdf = texture(aIntegratedBrdf, vec2(NoV, aRoughness)).rg;
    
    return filteredRadiance * (aSpecularColor * brdf.r + brdf.g);
}


/// @brief Split-sum approximatin of the specular contribution from IBL
/// Instead of fetching from the usual pre-computed textures,
/// this variant directly computes each term of the approximation.
vec3 approximateSpecularIbl_live(vec3 aSpecularColor,
                                 vec3 aReflection,
                                 float NoV,
                                 float aAlphaSquared,
                                 float aRoughness,
                                 samplerCube aEnvMap)
{
    vec3 prefilteredColor = prefilterEnvMapSpecular(aAlphaSquared, aReflection, aEnvMap);
    vec2 envBRDF = integrateBRDF(aAlphaSquared, max(0.00001, NoV));
    return prefilteredColor * (aSpecularColor * envBRDF.x + envBRDF.y);
}


#endif //include guard
