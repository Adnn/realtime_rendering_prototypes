#version 460


in vec2 ex_Uv;

uniform ivec2 u_FramebufferSize;
uniform int u_BlurRadius = 4;
uniform float u_DepthFactor = 10;
uniform float u_NormalFactor = 5;
 

uniform sampler2D u_Texture;
uniform sampler2D u_FragPosition_view;
uniform sampler2D u_FragNormal_view;

layout(location = 0) out vec4 out_Color;


void main()
{
	float frag_depth = texture(u_FragPosition_view, ex_Uv).z;
	vec3 frag_normal = texture(u_FragNormal_view, ex_Uv).xyz;

	// TODO: special care for the fragment value (its weight is 1)
	vec3 accu;
	float normalization = 0;
	for(int i = -u_BlurRadius; i <= u_BlurRadius; ++i)
	{
		for(int j = -u_BlurRadius; j <= u_BlurRadius; ++j)
		{
			vec2 offset = vec2(i, j) / u_FramebufferSize;
			vec2 sample_uv = ex_Uv + offset;
			// TODO: apply gaussian or something (and actually separate)
			vec4 sample_value = texture(u_Texture, sample_uv);

			float sample_depth = texture(u_FragPosition_view, sample_uv).z;
			vec3 sample_normal = texture(u_FragNormal_view, sample_uv).xyz;

			float distance = u_DepthFactor * (frag_depth - sample_depth);
			float distanceCloseness = 1 / (1 + distance * distance);

			float misalignment = u_NormalFactor * (1 - max(dot(frag_normal, sample_normal), 0.));
			float normalCloseness = 1 / (1 + misalignment * misalignment);

			float weight = normalCloseness * distanceCloseness;

			accu += sample_value.rgb * weight;
			normalization += weight;
		}
	}

	out_Color = vec4(accu / normalization, 1);

	// Shows how much samples tend to be weighted down by the bilateral aspect of the filter
	// (darker means total weight of samples is lower)
	//float totalSamples = pow(u_BlurRadius * 2 + 1, 2);
	//out_Color = vec4(vec3(normalization/totalSamples), 1);
}
