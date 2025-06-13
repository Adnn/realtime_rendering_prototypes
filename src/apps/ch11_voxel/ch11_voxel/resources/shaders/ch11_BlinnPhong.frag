#version 460

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/LightsBlock.glsl"
#include "shaders/LightUtilities.glsl"
#include "shaders/MaterialGenericBlock.glsl"


LightContributions applyBlinnPhongLight(
    vec3 aView, vec3 aLightDir, vec3 aShadingNormal,
    LightColors aColors, float aSpecularExponent)
{
    LightContributions result;

    float nDotL = dotPlus(aShadingNormal, aLightDir);
    result.diffuse = nDotL * aColors.diffuse.rgb;

    // Eliminate specular light bleeding, with a boolean multiplicative factor
    // see: https://computergraphics.stackexchange.com/q/14072/11110
    // Note: this has a major drawback: it introduces an abrupt cut-off between polygons
    // where the sign of nDotL changes, which is very disturbing for smooth surfaces.
    // Use a smoothstep windowing function instead of boolean value, to avoid discontinuity.
	float isFacingLight = smoothstep(0., 0.2, nDotL);

    vec3 h = normalize(aView + aLightDir);
    result.specular = isFacingLight 
                      * pow(dotPlus(aShadingNormal, h), aSpecularExponent)
                      * aColors.specular.rgb;

    return result;
}


in vec4 ex_Color;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;
in vec2 ex_Uv01;

uniform sampler2D u_DiffuseTexture;
uniform uint u_MaterialIdx;

out vec4 out_Color;


void main(void)
{
    MaterialGeneric material = ub_MaterialGeneric[u_MaterialIdx];

    vec4 albedo = ex_Color
        * texture(u_DiffuseTexture, ex_Uv01)
        ;

	//
    // alpha testing for cutout
    //
    if (albedo.a < 0.5)
    {
        discard;
    }

	vec3 view_cam = normalize(-ex_Position_view);
	vec3 shadingNormal_cam = normalize(ex_Normal_view);

    // Accumulators for the lights contributions
    vec3 diffuseAccum = vec3(0.);
    vec3 specularAccum = vec3(0.);

    //
    // Directional
    //
    for(uint directionalIdx = 0; directionalIdx != ub_DirectionalCount.x; ++directionalIdx)
    {
        DirectionalLight directional = ub_DirectionalLights[directionalIdx];
        vec3 lightDir_cam = -directional.direction.xyz;
        
        LightContributions lighting = 
            applyBlinnPhongLight(
                view_cam, lightDir_cam, shadingNormal_cam,
                directional.colors, material.specularExponent);

        diffuseAccum += lighting.diffuse;
        specularAccum += lighting.specular;
    }

    //
    // Point
    //
	for(uint pointIdx = 0; pointIdx != ub_PointCount.x; ++pointIdx)
    {
        PointLight point = ub_PointLights[pointIdx];

        // see rtr 4th p110 (5.10)
        vec3 lightRay_cam = point.position.xyz - ex_Position_view;
        float r = sqrt(dot(lightRay_cam, lightRay_cam));
        vec3 lightDir_cam = lightRay_cam / r;

        LightContributions lighting = 
            applyBlinnPhongLight(
                view_cam, lightDir_cam, shadingNormal_cam,
                point.colors, material.specularExponent);

        float falloff = attenuatePoint(point, r);
        diffuseAccum  += lighting.diffuse  * falloff;
        specularAccum += lighting.specular * falloff;
    }

    //
    // Sum contributions
    //
    vec3 ambient =  ub_AmbientColor.rgb * material.ambientColor.rgb  ;
    vec3 diffuse  = diffuseAccum        * material.diffuseColor.rgb  ;
    vec3 specular = specularAccum       * material.specularColor.rgb ;

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
