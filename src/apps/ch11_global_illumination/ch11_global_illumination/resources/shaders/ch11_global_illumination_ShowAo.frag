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

uniform bool u_ScreenSpaceNonLinearDepth = false;

layout(location = 0) out vec4 out_Color;

void main(void)
{
	// UV coordinate of this fragment, mapping window space to [0, 1]^2 
	const vec2 frag_uv = gl_FragCoord.xy / u_FramebufferSize;

	// UV coordinate in the noise texture, matching texel to fragment 1:1
	const vec2 noise_uv = gl_FragCoord.xy / textureSize(u_NoiseDirections, 0);
	vec3 reflectionPlaneNormal = texture(u_NoiseDirections, noise_uv).xyz;

	float fragVisibility = 0;
	float totalWeight  = 0;
	for(uint i = 0; i != SSAO_SAMPLE_COUNT; ++i)
	{
		vec3 sphereSample = u_SsaoSamples[i];
		if (u_ReflectSamples)
		{
			sphereSample = reflect(sphereSample, reflectionPlaneNormal);
		}
		vec3 offset = u_SphereRadius * sphereSample;

		float weight = 1;
		if(u_Weighted)
		{
			// Weighting factor taken from iquilez
			float zd = u_WeightFactor * length(sphereSample);
			weight = 1 / (1 + zd * zd);
		}
		totalWeight += weight;

		if(u_SphereInScreenSpace)
		{
			// The offset is divided by 2 to match with the uv which 
			// is already remapped from [-1, 1] to [0, 1]
			vec2 sample_uv = frag_uv + (offset.xy / 2);

			if (u_ScreenSpaceNonLinearDepth)
			{
				// Note: Here we sum the fragcoord.z (which has been transformed by projection to non-linear depth space)
				// with a linear offset, which probably introduces a lot of error.

				// The depth map is "left handed", in the sense that higher value means further away.
				// With a texture compare of GL_LESS,  texture() will return 1 if the value
				// in the texture is superior or equal to the provided reference value (as coordinate w)
				// It means it will return 1 if the sample is **NOT** occluded == summing visibility.
				// Note: we subtract the offset and bias here (left handed) to match the other comparisons (right handed)
				fragVisibility += texture(u_DepthMap, vec3(sample_uv, gl_FragCoord.z - offset.z - u_DepthBias)) * weight;
			}
			else
			{
				float sampleClosestDepth_view = texture(u_FragPosition_view, sample_uv).z;
				// If the right handed coordinate, if closest surface is superior to the sample depth
				// it means the sample is obstructed.
				if (sampleClosestDepth_view <= (ex_Position_view.z + offset.z + u_DepthBias))
				{
					fragVisibility += weight;
				}
			}
		}
		else // Offset the sample in view space (requires transformation and division)
		{
			vec3 sample_view = ex_Position_view + offset;

			// project sample position
			vec4 sample_clip = ub_projection * vec4(sample_view, 1);
			vec3 sample_ndc = sample_clip.xyz / sample_clip.w;
			// Remap [-1, 1] to [0, 1]
			vec2 sample_uv = (sample_ndc.xy + 1) / 2;

			float sampleClosestDepth_view = texture(u_FragPosition_view, sample_uv).z;
			// If the **right** handed coordinate of view space,
			// if closest surface is superior to the sample depth it means the sample is obstructed.
			if (sampleClosestDepth_view <= (sample_view.z + u_DepthBias))
			{
				fragVisibility += weight;
			}
		}

	}

	fragVisibility /= totalWeight;
    out_Color = vec4(vec3(fragVisibility), 1);
}
