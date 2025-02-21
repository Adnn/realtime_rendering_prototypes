#version 460

#include "Gamma.glsl"
#include "Helpers.glsl"
#include "LightsBlock.glsl"
#include "LightUtilities.glsl"
#include "MaterialsBlock.glsl"

// We go for the lambertian shading model:
// $$
// C_shaded = \sum{i \inc lights}{(l_i \cdot n)^+ * C_{light_i} * C_{surface}}
// $$
// For Lambertian surfaces, we have: $ L_o(v) = \frac{\rho_ss}{\pi} * E $

// see:: Rtr 4th p382 (10.9)
/// @param aWrapK: Wrap factor from 0 (point light) to 1 (entire hemisphere)
/// @return the warped dot product (the irradiance E multiplied by Pi/C_light)
float wrapDot_Forsyth(vec3 aLightDir, vec3 aShadingNormal, float aWrapK)
{
    return max(
        0.,
        (dot(aLightDir, aShadingNormal) + aWrapK) / (1 + aWrapK));
}


// see: Karis, Brian, "Real Shading in Unreal Engine 4," p15
/// @return A modified light direction
vec3 representativePoint_sphere(vec3 aFragmentPosition,
                                vec3 aLightCenter,
                                vec3 aReflection,
                                float aRadius)
{
    // shaded point to light-sphere center
    vec3 L = aLightCenter - aFragmentPosition;

    // Note: here, Kaaris use the negation of what is presented in rtr 4th fig 10.10
    // We negate Kaaris formulation to get the correct result with our conventions.
    // From the sphere center to the closest point on the reflection ray:
    vec3 centerToRay = dot(L, aReflection) * aReflection - L;
    // Note: the paper use single bars around center to ray, we take it to mean the norm
    vec3 closestPoint = L + centerToRay * clamp(aRadius / length(centerToRay), 0., 1.);
    // Note: here, the paper use double bars, but result is a vector. We take it to mean normalization.
    return normalize(closestPoint);

    // Note: below is rtr 4th fig 10.10 formalization:
    //vec3 pcr = dot(L, aReflection) * aReflection - L;
    //vec3 pcs = L + pcr * min(1, aRadius / length(pcr));
    //return normalize(pcs);
}


struct LightContributions
{
	vec3 diffuse;
	vec3 specular;
};


vec3 applyDiffuse_wrap(
    vec3 aLightDir,
    vec3 aShadingNormal,
    LightColors aColors,
    float aWrapK)
{
    float nDotL = wrapDot_Forsyth(aLightDir, aShadingNormal, aWrapK);
    return nDotL * aColors.diffuse.rgb;
}

vec3 applyDiffuse_phong(
    vec3 aLightDir,
    vec3 aShadingNormal,
    LightColors aColors)
{
    float nDotL = dotPlus(aShadingNormal, aLightDir);
    return nDotL * aColors.diffuse.rgb;
}

vec3 applySpecular(
    vec3 aView, vec3 aLightDir, vec3 aShadingNormal,
    LightColors aColors, float aSpecularExponent)
{
    float nDotL = dotPlus(aShadingNormal, aLightDir);

    // Eliminate specular light bleeding, with a boolean multiplicative factor
    // see: https://computergraphics.stackexchange.com/q/14072/11110
    // Note: this has a major drawback: it introduces an abrupt cut-off between polygons
    // where the sign of nDotL changes, which is very disturbing for smooth surfaces.
    // Use a smoothstep windowing function instead of boolean value, to avoid discontinuity.
	float isFacingLight = smoothstep(0., 0.02, nDotL);

    vec3 h = normalize(aView + aLightDir);
    return isFacingLight 
           * pow(dotPlus(aShadingNormal, h), aSpecularExponent)
           * aColors.specular.rgb;
}

in vec4 ex_Color;
in vec3 ex_Normal;
in vec3 ex_Position;

out vec4 out_Color;


void main(void)
{
    Material material = ub_Materials[0];

    // TODO: multiply by albedo texture
    vec4 albedo = ex_Color;

	vec3 view_cam = normalize(-ex_Position);
	vec3 shadingNormal_cam = normalize(ex_Normal);

    // Accumulators for the lights contributions
    vec3 diffuseAccum = vec3(0.);
    vec3 specularAccum = vec3(0.);

    //
    // Point
    //
	for(uint pointIdx = 0; pointIdx != ub_PointCount.x; ++pointIdx)
    {
        PointLight point = ub_PointLights[pointIdx];

        // see rtr 4th p110 (5.10)
        vec3 lightRay_cam = point.position.xyz - ex_Position;
        float r = sqrt(dot(lightRay_cam, lightRay_cam));
        vec3 lightDir_cam = lightRay_cam / r;

        LightContributions lighting;

//#define FEAT_WRAP_LIGHTING
#ifdef FEAT_WRAP_LIGHTING
		lighting.diffuse = applyDiffuse_wrap(
            lightDir_cam, shadingNormal_cam, point.colors, point.wrapK);
#else
		lighting.diffuse = applyDiffuse_phong(
            lightDir_cam, shadingNormal_cam, point.colors);
#endif //FEAT_WRAP_LIGHTING

        vec3 representativeLightDir_cam =
            representativePoint_sphere(ex_Position, point.position.xyz,
                                       reflect(-view_cam, shadingNormal_cam), point.radius.x);
        vec3 lightSpecularDir = representativeLightDir_cam;
        //vec3 lightSpecularDir = lightDir_cam;

		lighting.specular = applySpecular(
			view_cam, lightSpecularDir, shadingNormal_cam,
			point.colors, material.specularExponent);

        // attenuatePoint() will use r0 as light radius:
        // this is the exact solution for the diffuse contribution of a sphere light
        // of radius r0.
        float falloff = attenuatePoint(point, r);
        diffuseAccum  += lighting.diffuse  * falloff;
        specularAccum += lighting.specular * falloff;
    }

    //
    // Sum contributions
    //
    vec3 ambient =  ub_AmbientColor.rgb * material.ambientColor.rgb;
    vec3 diffuse  = diffuseAccum        * material.diffuseColor.rgb;
    vec3 specular = specularAccum       * material.specularColor.rgb;

#if FEAT_ALBEDO_SPECULAR
    // The specular highlights are multiplied with the albedo:
    // this gives tinted specular reflection (correct for metals, wrong for dielectrics), 
    // and dim highlights for dark surfaces.
    vec3 phongColor = albedo.rgb * (diffuse + specular + ambient);
#else
    vec3 phongColor = albedo.rgb * (diffuse + ambient) + specular;
#endif

	out_Color = correctGamma(vec4(phongColor, albedo.a));
}
