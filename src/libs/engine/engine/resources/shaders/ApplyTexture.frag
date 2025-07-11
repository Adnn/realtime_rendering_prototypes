#version 460

#include "shaders/Helpers.glsl"

#define MODE_LINEARIZE_DEPTH 1
#define MODE_DIRECTION 2
#define MODE_DEPTH_FROM_POSITION 3
#define MODE_RAW_RED_CHANNEL 4

in vec2 ex_Uv;

uniform uint u_Mode;

uniform sampler2D u_Texture;

uniform float u_NearDistance;
uniform float u_FarDistance;

out vec4 out_Color;


void main(void)
{
	// This approach will map the whole texture to the whole window
	// (without consideration for aspect ratio or resolution differences)
	vec2 uv = ex_Uv;
	vec4 value = texture(u_Texture, uv);

	switch(u_Mode)
	{
		case MODE_LINEARIZE_DEPTH:
			out_Color = vec4(vec3(linearizeDepth(value.r, u_NearDistance, u_FarDistance)), 1);
			break;
		case MODE_DIRECTION:
			out_Color = vec4(mapToUnit(value.rgb), 1);
			break;
		case MODE_DEPTH_FROM_POSITION:
			out_Color = vec4(
				vec3((value.z - u_NearDistance) / (u_FarDistance - u_NearDistance)),
				1);
			break;
		case MODE_RAW_RED_CHANNEL:
			out_Color = vec4(vec3(value.r), 1);
			break;
	}
}
