#version 460

#include "Gamma.glsl"
#include "Helpers.glsl"
#include "LightsBlock.glsl"


struct LightContributions
{
	vec3 diffuse;
	vec3 specular;
};


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


struct Material
{
	float specularExponent;
    vec4 ambientColor;
    vec4 diffuseColor;
    vec4 specularColor;
};


in vec3 ex_Color;
in vec3 ex_Normal;
in vec3 ex_Position;

out vec4 out_Color;


void main(void)
{
    // TODO: Handle surface materials
    Material material;
    material.specularExponent = 20;
    material.ambientColor = vec4(1.);
    material.diffuseColor = vec4(1.);
    material.specularColor = vec4(1.);

    // TODO: multiply by albedo texture
    vec4 albedo = vec4(ex_Color, 1.);

	vec3 view_cam = normalize(-ex_Position);
	vec3 shadingNormal_cam = normalize(ex_Normal);

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
