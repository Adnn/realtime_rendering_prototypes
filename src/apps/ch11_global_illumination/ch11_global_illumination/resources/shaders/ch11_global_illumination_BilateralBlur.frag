#version 460


in vec2 ex_Uv;

uniform ivec2 u_FramebufferSize;
uniform int u_BlurRadius = 4;

uniform sampler2D u_Texture;

layout(location = 0) out vec4 out_Color;


void main()
{
	vec3 accu;
	float normalization = 0;
	for(int i = -u_BlurRadius; i <= u_BlurRadius; ++i)
	{
		for(int j = -u_BlurRadius; j <= u_BlurRadius; ++j)
		{
			vec2 offset = vec2(i, j) / u_FramebufferSize;

			// TODO: apply gaussian or something (and actually separate)
			vec4 value = texture(u_Texture, ex_Uv + offset);

			// TODO: bilateral on z difference, on normal difference, ...
			//float closeness = 1.0f;

			float weight = 1.0f;

			accu += value.rgb * weight;
			normalization += weight;
		}
	}

	out_Color = vec4(accu / normalization, 1);
}
