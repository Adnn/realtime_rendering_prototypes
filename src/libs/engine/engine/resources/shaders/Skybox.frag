#version 430

#include "Constants.glsl"
#include "Gamma.glsl"
#include "Helpers.glsl"

in vec3 ex_FragmentPosition_world;

#if defined(EQUIRECTANGULAR)
uniform sampler2D u_EnvironmentTexture;
#else
uniform samplerCube u_EnvironmentTexture;
#endif

uniform float u_LodBias = 0;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec3 out_LinearHdr;


void main()
{
#if defined(EQUIRECTANGULAR)
    vec3 envColor = texture(u_EnvironmentTexture, worldToEquirectangular(ex_FragmentPosition_world), u_LodBias).rgb;

#else // Cubemap
    // Cubemap coordinate system is left handed, but the cube texture coords is given
    // in right handed world space, so negate Z. 
    // Note: the individual images in the cubemap texture are loaded "upside-down" compared to usual OpenGL textures
    // in order for this to work (you can see they are upside down in Nsight Graphics)
    vec3 sampleDir = worldToCubemap(ex_FragmentPosition_world);

//#define MANUAL_LOD
#if defined(MANUAL_LOD)
    // TODO: we do not get the approach results, investigate
    // LOD formula as provided in GL 4.6 spec, formulas 8.7 (p256), 8.10 (p257) and 8.11 (p258)
    vec3 uv = sampleDir * vec3(textureSize(u_EnvironmentTexture, 0), 0);
	float lod = log2(max( sqrt(dot(dFdx(uv), dFdx(uv))),
                          sqrt(dot(dFdy(uv), dFdy(uv))) ));
    vec3 envColor = textureLod(u_EnvironmentTexture, sampleDir, lod + u_LodBias).rgb;
	float debugQueryLod = lod;
#else
    vec3 envColor = texture(u_EnvironmentTexture, sampleDir, u_LodBias).rgb;
	float debugQueryLod = textureQueryLod(u_EnvironmentTexture, sampleDir).x;
#endif // MANUAL_LOD

//#define DEBUG_LOD
#if defined(DEBUG_LOD)
	{
		envColor = vec3(debugQueryLod / textureQueryLevels(u_EnvironmentTexture));
	}
#endif // DEBUG_LOD

#endif // EQUIRECTANGULAR

    out_Color = correctGamma(vec4(envColor, 1.0));
    out_LinearHdr = envColor;
}
