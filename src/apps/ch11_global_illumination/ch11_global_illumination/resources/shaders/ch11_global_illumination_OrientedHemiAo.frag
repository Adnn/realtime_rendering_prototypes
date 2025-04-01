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

uniform sampler2D u_FragPosition_view;
uniform sampler2D u_FragNormal_view;

// Random normals (already unit length)
uniform sampler2D u_NoiseDirections;

uniform ivec2 u_FramebufferSize;
uniform vec3 u_SsaoSamples[SSAO_SAMPLE_COUNT];

// Controls
uniform float u_DepthBias;
// TODO: rename to match the semantic of rotation here
uniform bool u_ReflectSamples;
uniform bool u_Weighted;
uniform float u_SphereRadius;
uniform float u_WeightFactor = 1;

layout(location = 0) out vec4 out_Color;


// Return the TBN matrix: transforms coordinates from TBN space to the space of provided arguments
// (usually view space or world space)
mat3 prepareTbn(vec3 aN, vec3 aT)
{
	// TODO: make robust to parallel N and T
	vec3 tangent = normalize(aT - aN * dot(aN, aT));
	vec3 bitangent = cross(aN, tangent);
    return mat3(tangent, bitangent, aN);
}


void main(void)
{
	// UV coordinate of this fragment, mapping window space to [0, 1]^2 
	const vec2 frag_uv = gl_FragCoord.xy / u_FramebufferSize;

	vec3 normal_view = normalize(texture(u_FragNormal_view, frag_uv).xyz);

	// UV coordinate in the noise texture, matching texel to fragment 1:1
	const vec2 noise_uv = gl_FragCoord.xy / textureSize(u_NoiseDirections, 0);
	// TODO: use an already normalized vector in XY plane

	vec3 tangentApprox_view = u_ReflectSamples ?
		normalize(vec3(texture(u_NoiseDirections, noise_uv).xy, 0))
		: vec3(1, 0, 0);
	// Construct TBN basis
	mat3 tbn = prepareTbn(normal_view, tangentApprox_view);

	float fragVisibility = 0;
	float totalWeight  = 0;
	for(uint i = 0; i != SSAO_SAMPLE_COUNT; ++i)
	{
		vec3 sphereSample = u_SsaoSamples[i];
		vec3 offset_view = tbn * (u_SphereRadius * sphereSample);

		float weight = 1;
		if(u_Weighted)
		{
			// Weighting factor taken from iquilez
			float zd = u_WeightFactor * length(sphereSample);
			weight = 1 / (1 + zd * zd);
		}
		totalWeight += weight;

		vec3 sample_view = ex_Position_view + offset_view;

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

	fragVisibility /= totalWeight;
    out_Color = vec4(vec3(fragVisibility), 1);
}
