#if !defined(SHADOW_GLSL_INCLUDE_GUARD)
#define SHADOW_GLSL_INCLUDE_GUARD


#include "shaders/LightUtilities.glsl"


uniform sampler2DShadow u_ShadowMap;
uniform samplerCubeShadow u_OmniShadowMap;
in vec3[MAX_SHADOW_MAPS] ex_Position_lightTex;

uniform float u_ShadowCubeNearDistance;
uniform float u_ShadowCubeFarDistance;


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


void applyShadowToDirectionalLighting(
    inout LightContributions aLighting,
    uint aDirectionalIdx)
{
    // TODO: extend to handle several lights
	float shadowAttenuation = 
		getShadowAttenuation(ex_Position_lightTex[aDirectionalIdx]);
	scale(aLighting, shadowAttenuation);
}


void applyShadowToPointLighting(
    inout LightContributions aLighting,
    uint aPointIdx,
    vec3 aFragment_world)
{
	vec3 light_world = ub_PointLights[aPointIdx].position.xyz;
	vec3 samplingRay = aFragment_world - light_world.xyz;
	float fragDistance =
		mapRangeToUnit(length(samplingRay),
                       u_ShadowCubeNearDistance, u_ShadowCubeFarDistance);
	float shadowAttenuation = 
        texture(u_OmniShadowMap, 
                vec4(worldToCubemap(samplingRay),
                     //shadowMapIdx, // array layer
                     fragDistance));
	scale(aLighting, shadowAttenuation);
}


#endif //SHADOW_GLSL_INCLUDE_GUARD