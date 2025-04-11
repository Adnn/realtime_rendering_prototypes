#version 460

#include "Gamma.glsl"
#include "Helpers.glsl"
#include "LightsBlock.glsl"
#include "LightUtilities.glsl"
#include "MaterialPbrBlock.glsl"
#include "PbrUtilities.glsl"



LightContributions applyLight_pbr(vec3 aView, vec3 aDiffuseLightDir, vec3 aSpecularLightDir, vec3 aShadingNormal,
                                  PbrParameters aParams, LightColors aColors)
{
    LightContributions result;
    vec3 F;

    // IMPORTANT: All albedos are given already multiplied by Pi
    // This already satisfy the Pi factor in the reflectance equation.

    // Specular and Fresnel
    {
		vec3 aLightDir = aSpecularLightDir;

        // Note: Lacking clear guidance, the Fresnel term is computed with the specular light dir
        //   The reasoning being that for practical purposes, this is the specular direction
        //   (and we want the energy trade-off with diffuse for the computed specular term)

        // TODO #glitch: The area light representative point made visible black dot artifacts on the
        //   sphere horizon when it is aligned to the light ("eclipse").
        //   I suppose one problem is the degenerate h vector when aView = -aLightDir (more probable with area)
        //   but there seem to be other underlying issue(s).
		vec3 h = normalize(aView + aLightDir);
		float hDotL = dotPlus(h, aLightDir);

		// Fresnel term `F` describe how the wave-length dependent reflectance (proportion of reflected light)
		// For microfacet BRDFs, we use dot(h, l), not dot(n, l), see: rtr 4th eq (9.63)
		F = schlickFresnelReflectance(hDotL, aParams.f0, aParams.f90);

        float nDotH = dotPlus(aShadingNormal, h);
        // Seems to fix some erroneous black pixels at horizon (but not all)
        float nDotL = max(0.001, dotPlus(aShadingNormal, aLightDir));
        float nDotV = dotPlus(aShadingNormal, aView);

        #if !defined(BLINNPHONG_BRDF)
            result.specular = specularBrdf_GGX(F, nDotH, nDotL, nDotV, aParams.alpha)
                              * aColors.specular.rgb
                              * nDotL
                              ;
        #else
            float nDotL_raw = dot(aShadingNormal, aLightDir);
            float nDotV_raw = dot(aShadingNormal, aView);

            float alpha_b = alpha / 1.7; // the magic denominator was manually tweaked to mostly match
            result.specular = specularBrdf_BlinnPhong(F, nDotH, nDotL_raw, nDotV_raw, alpha_b)
                              * aColors.specular.rgb
                              * nDotL;
        #endif // GGX_BRDF / BLINNPHONG_BRDF

		//result.specular = vec3(-h.g);
    }

    // Diffuse
    {
        // Note: From rtr 4th structure and references to specular in Karis, we assume that 
        //   diffuse term should still use the punctual light direction
        //   (also, it seems wrong otherwise).
        vec3 aLightDir = aDiffuseLightDir;

        float nDotL = dotPlus(aShadingNormal, aLightDir);
        result.diffuse  = diffuseBrdf_weightedLambertian(F, aParams.diffuseColor)
                          * aColors.diffuse.rgb
                          * nDotL;
    }

    return result;
}


in vec4 ex_Color;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;

out vec4 out_Color;


void main(void)
{
    MaterialPbr material = ub_MaterialPbr[0];

    // TODO: multiply by albedo texture
    vec4 albedo = ex_Color * material.baseColor;

    vec3 viewDir_view = normalize(-ex_Position_view);
    // TODO: normal mapping
    vec3 shadingNormal_view = normalize(ex_Normal_view);

    // Accumulators for the lights contributions
    vec3 diffuseAccum = vec3(0.);
    vec3 specularAccum = vec3(0.);


    //
    // Metallic-Roughness parameterization 
    //

    // TODO: implement mrao texture
    float metallic = material.metallicRoughness.x;
    float roughness = material.metallicRoughness.y;

    // Handle alpha
    // We assume the roughness, not alpha, is provided even in 3rd party assets.
    float alpha = alphaFromRoughness(roughness);

    // Note: Too smooth a surface (i.e too low an alpha)
    // makes it that there is not even a specular highlight showing with most models
    // (at least GGX & Blinn-Phong)
    // So, uncomment to fix it (e.g. the display on the glTF water bottle)
    //alpha = max(0.005, alpha);

    // We blendg the parameters before computing the lighting model.
    // This is not physically correct (parameters do not have linear relationship to output)
    // but this is fast and results are still convincing. 
    PbrParameters pbrParameters;
    pbrParameters.diffuseColor = mix(albedo.rgb, vec3(0.), metallic);
    pbrParameters.f0 = mix(gF0_dielec, albedo.rgb, metallic);
    pbrParameters.f90 = gF90;
    pbrParameters.alpha = alpha;


    // 
    // PBR shading model (light simulation)
    //

    // TODO: shadows

    // Directional lights
    for(uint directionalIdx = 0; directionalIdx != ub_DirectionalCount.x; ++directionalIdx)
    {
        DirectionalLight directional = ub_DirectionalLights[directionalIdx];
        vec3 lightDir_view = -directional.direction.xyz;
        
        LightContributions lighting = 
            applyLight_pbr(
                viewDir_view, lightDir_view, lightDir_view, shadingNormal_view,
                pbrParameters, directional.colors);

        diffuseAccum += lighting.diffuse;
        specularAccum += lighting.specular;
    }


    //// Point lights
    //for(uint pointIdx = 0; pointIdx != ub_PointCount.x; ++pointIdx)
    //{
    //    PointLight point = ub_PointLights[pointIdx];

    //    // see rtr 4th p110 (5.10)
    //    vec3 lightRay_view = point.position.xyz - ex_Position_view;
    //    float radius = length(lightRay_view);
    //    vec3 lightDir_view = lightRay_view / radius;

    //    vec3 specularLightDir_view = normalize(
    //        representativePoint_sphere(ex_Position_view,
    //                                   point.position.xyz,
    //                                   reflect(-viewDir_view, shadingNormal_view),
    //                                   point.radius.x));

    //    LightContributions lighting = 
    //        applyLight_pbr(
    //            viewDir_view, lightDir_view, specularLightDir_view, shadingNormal_view,
    //            pbrParameters, point.colors);

    //    float falloff = attenuatePoint(point, radius);
    //    diffuseAccum  += lighting.diffuse  * falloff;
    //    specularAccum += lighting.specular * falloff;
    //}


    // Tube lights
    for(uint pointIdx = 0; pointIdx != ub_PointCount; pointIdx += 2)
    {
        PointLight p0 = ub_PointLights[pointIdx];
        PointLight p1 = ub_PointLights[pointIdx+1];

        // TODO: #area_diffuse
        //       Find if there are better solution than picking the middle point
        vec3 middlePoint_view = (p0.position.xyz + p1.position.xyz) / 2;
        vec3 midLightRay_view = middlePoint_view - ex_Position_view;
        float midRadius = length(midLightRay_view);
        vec3 midLightDir_view = midLightRay_view / midRadius;

		vec3 reflectionDir = reflect(-viewDir_view, shadingNormal_view);

        TubeInterpolation tube = 
            representativePoint_tube(
                ex_Position_view,
                p0.position.xyz, p1.position.xyz,
                reflectionDir);

        vec3 lightRay_view = tube.lightRay;
        float t = tube.t;

        #define FEAT_TUBE_WIDTH
        #if defined(FEAT_TUBE_WIDTH)
			lightRay_view = 
				representativePoint_sphere(lightRay_view,
										   reflectionDir,
										   p0.radius.x);
        #endif

        float representativeRadius = length(lightRay_view);
        vec3 lightDir_view = lightRay_view / representativeRadius;

        LightContributions lighting = 
            applyLight_pbr(
                viewDir_view, midLightDir_view, lightDir_view, shadingNormal_view,
                pbrParameters,
                // TODO: interpolate colors
                p0.colors);

		// TODO interpolate point fall-off radii?
        // Note: we cannot use the radius of the representative point:
        // It has strong discontinuities when "escaping" from endpoints, 
        // which cause disturbing artifacts in a "low frequency" diffuse contribution.
        float falloffDiffuse = attenuatePoint(p0, midRadius);
        diffuseAccum  += lighting.diffuse  * falloffDiffuse;

        // Note: we might instead reuse the already computed falloff, 
        // but this change the specular result
        float falloffSpec = attenuatePoint(p0, representativeRadius);
        specularAccum += lighting.specular * falloffSpec;

        // Usefull to show the interpolation parameter
        // gamma encode t to go from a perceptually linear t to light linear space.
        // (this cancels out gamma correction, bringing it back to perceptually linear sRGB)
        //out_Color = correctGamma(vec4(vec3(pow(t, 2.2)), 1));
        //out_Color = correctGamma(vec4(vec3(pow(representativeRadius/10, 2.2)), 1));
        //return;
    }


    // Sum contributions
    // Note: the ambient term is a quick hack, to be removed when IBL is in place
    // We multiply it by the diffuse color, so metals do not have ambient terms, and dielectrics have their tint.
    vec3 ambient =  ub_AmbientColor.rgb * material.ambientColor.rgb * pbrParameters.diffuseColor;
    vec3 diffuse  = diffuseAccum        ;//* material.diffuseColor.rgb;
    vec3 specular = specularAccum       ;//* material.specularColor.rgb;

    vec3 fragmentColor = diffuse + ambient + specular;

    // DEBUG SECTION
    //fragmentColor = specular;
    //fragmentColor = highlightAberrations(fragmentColor);

    //
    // Output
    //
    out_Color = correctGamma(vec4(fragmentColor, albedo.a));
}
