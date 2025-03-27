#version 460

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/LightsBlock.glsl"
#include "shaders/LightUtilities.glsl"
#include "shaders/MaterialPbrBlock.glsl"
#include "shaders/PbrUtilities.glsl"
#include "shaders/ViewProjectionBlock.glsl"

in vec4 ex_Color;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;

uniform sampler2DShadow u_DepthMap;
uniform sampler2D u_FragPosition_view;

// Random normals (already unit length)
uniform sampler2D u_NoiseDirections;
uniform ivec2 u_FramebufferSize;
uniform vec3 u_SsaoSamples[SSAO_SAMPLE_COUNT];

// Controls
uniform float u_DepthBias;
uniform bool u_ReflectSamples;
uniform bool u_Weighted;
uniform float u_SphereRadius;
uniform bool u_SphereInScreenSpace;
uniform float u_WeightFactor = 1;

layout(location = 0) out vec4 out_Color;

void main(void)
{
#if defined(FIRST)
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

#else
	// UV coordinate in the noise texture, matching texel to fragment 1:1
	const vec2 noise_uv = gl_FragCoord.xy / textureSize(u_NoiseDirections, 0);
	vec3 reflectionPlaneNormal = texture(u_NoiseDirections, noise_uv).xyz;

	// A higher occlusion means the fragment is more occluded
	float occlusion = 0;
	float totalWeight  = 0;
	for(uint i = 0; i != SSAO_SAMPLE_COUNT; ++i)
	{
		vec3 sphereSample = u_SsaoSamples[i];

		if (u_ReflectSamples)
		{
			sphereSample = reflect(sphereSample, reflectionPlaneNormal);
		}

		vec3 offset = u_SphereRadius * sphereSample;

		vec3 sample_view = ex_Position_view;
		// Depending on the value of this uniform, we add the offset before of after projection
		// and perspective division
		if(!u_SphereInScreenSpace)
		{
			sample_view += offset;
		}

		// project sample position
		vec4 sample_clip = ub_projection * vec4(sample_view, 1);
		vec3 sample_ndc = sample_clip.xyz / sample_clip.w;
		if(u_SphereInScreenSpace)
		{
			sample_ndc += offset;
			sample_view.z += offset.z;
		}
		vec2 sample_screenuv = (sample_ndc.xy + 1) / 2;

		float weight = 1;
		if(u_Weighted)
		{
			// Weighting factor taken from iquilez
			float zd = u_WeightFactor * length(sphereSample);
			weight = 1 / (1 + zd * zd);
		}
		totalWeight += weight;

		float sampleClosestDepth_view = texture(u_FragPosition_view, sample_screenuv).z;
		// If the right handed coordinate, if closest surface is superior to the sample depth
		// it means the sample is obstructed.
		if (sampleClosestDepth_view > (sample_view.z + u_DepthBias))
		{
			occlusion += weight;
		}
	}
	occlusion /= totalWeight;
	//occlusion /= SSAO_SAMPLE_COUNT;
	float accu = 1 - occlusion;

#endif

    out_Color = vec4(vec3(accu), 1);
}
