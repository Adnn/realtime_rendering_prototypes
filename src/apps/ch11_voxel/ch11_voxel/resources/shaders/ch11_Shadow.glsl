#if !defined(SHADOW_GLSL_INCLUDE_GUARD)
#define SHADOW_GLSL_INCLUDE_GUARD


#include "shaders/Helpers.glsl"
#include "shaders/LightUtilities.glsl"
#include "shaders/LightViewProjectionBlock.glsl"


uniform sampler2DArrayShadow u_ShadowMap;
uniform samplerCubeArrayShadow u_OmniShadowMap;
in VertexProjection ex_Position_lightTex;

uniform float u_ShadowCubeNearDistance;
uniform float u_ShadowCubeFarDistance;


// Bias is implemented via polygon offset
float getShadowAttenuationDirectionalLighting(uint aDirectionalIdx)
{
    vec3 position_lightTex = ex_Position_lightTex.position[aDirectionalIdx];
    return texture(u_ShadowMap, 
                   vec4(position_lightTex.xy, // uv
                        aDirectionalIdx, // array layer
                        position_lightTex.z /* reference value */));
}


void applyShadowToDirectionalLighting(
    inout LightContributions aLighting,
    uint aDirectionalIdx)
{
	float shadowAttenuation = 
		getShadowAttenuationDirectionalLighting(aDirectionalIdx);
	scale(aLighting, shadowAttenuation);
}


float getShadowAttenuationPointLighting(uint aPointIdx, vec3 aFragment_world)
{
	vec3 light_world = ub_PointLights[aPointIdx].position.xyz;
	vec3 samplingRay = aFragment_world - light_world.xyz;
	float fragDistance =
		mapRangeToUnit(length(samplingRay),
                       u_ShadowCubeNearDistance, u_ShadowCubeFarDistance);
	return texture(u_OmniShadowMap, 
                   vec4(worldToCubemap(samplingRay),
                        aPointIdx), // array layer
                   // Note: for cubemap array shadow sampler need 5 "coordinates",
                   // so the comparison value is taken as a separate parameter instead of extending the coordinate vector
                   fragDistance);
 }


void applyShadowToPointLighting(
    inout LightContributions aLighting,
    uint aPointIdx,
    vec3 aFragment_world)
{
	float shadowAttenuation =
        getShadowAttenuationPointLighting(aPointIdx, aFragment_world);
	scale(aLighting, shadowAttenuation);
}


#endif //SHADOW_GLSL_INCLUDE_GUARD