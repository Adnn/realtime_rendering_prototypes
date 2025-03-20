#version 460

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/LightsBlock.glsl"
#include "shaders/LightUtilities.glsl"
#include "shaders/MaterialPbrBlock.glsl"
#include "shaders/PbrUtilities.glsl"


in vec4 ex_Color;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;

uniform sampler2DShadow u_DepthMap;
// Random normals (already unit length)
uniform sampler2D u_NoiseDirections;
uniform ivec2 u_FramebufferSize;
uniform vec3 u_SsaoSamples[SSAO_SAMPLE_COUNT];

out vec4 out_Color;

void main(void)
{
	// UV coordinate of this fragment, mapping window space to [0, 1]^2 
	const vec2 frag_uv = gl_FragCoord.xy / u_FramebufferSize;

	// UV coordinate in the noise texture, matching texel to fragment 1:1
	const vec2 noise_uv = gl_FragCoord.xy / textureSize(u_NoiseDirections, 0);
	vec3 reflectionPlaneNormal = texture(u_NoiseDirections, noise_uv).xyz;

	const float offset_scale = 0.01f;
	float accu = 0;
	float w = 0;
	for(uint i = 0; i != SSAO_SAMPLE_COUNT; ++i)
	{
		vec3 sphereSample = u_SsaoSamples[i];

		#define REFLECT
		#if defined(REFLECT)
			sphereSample = reflect(sphereSample, reflectionPlaneNormal);
		#endif

		vec3 offset = offset_scale * sphereSample;
		vec2 uv = frag_uv + offset.xy;

		#define WEIGHT
		#if defined(WEIGHT)
			// Taken from iquilez
			float zd = 4 * length(u_SsaoSamples[i]);
			float weight = 1 / (1 + zd * zd);
		#else
			float weight = 1;
		#endif

		// TODO: I probably cannot sum the fragcoord.z (which is transformed by projection)
		// with some linear z offset.
		accu += texture(u_DepthMap, vec3(uv, gl_FragCoord.z + offset.z)) * weight;
		w += weight;
	}
	accu /= w;

    out_Color = correctGamma(vec4(vec3(accu), 1));
}
