#if !defined(SHADOW_GLSL_INCLUDE_GUARD)
#define SHADOW_GLSL_INCLUDE_GUARD


#include "shaders/LightUtilities.glsl"


uniform sampler2DShadow u_ShadowMap;
in vec3[MAX_SHADOW_MAPS] ex_Position_lightTex;


// Bias is implemented via polygon offset
float getShadowAttenuation(
    vec3 fragPosition_lightTex
    //uint shadowMapIdx
    )
{
    return texture(u_ShadowMap, 
                   vec3(fragPosition_lightTex.xy, // uv
                        //shadowMapIdx, // array layer
                        fragPosition_lightTex.z /* reference value */));
}


void applyShadowToLighting(
    inout LightContributions aLighting,
    uint aDirectionalIdx)
{
    // TODO: extend to handle several lights
	float shadowAttenuation = 
		getShadowAttenuation(ex_Position_lightTex[aDirectionalIdx]);
	scale(aLighting, shadowAttenuation);
}


#endif //SHADOW_GLSL_INCLUDE_GUARD