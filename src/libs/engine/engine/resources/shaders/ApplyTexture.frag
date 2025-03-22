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


// Linearization functions 

float linearizeDepth(float aDepthBufferValue)
{
	// Remap depth value [0, 1] to NDC [-1, 1]
	float d = 2 * aDepthBufferValue - 1.0;
	return
		(2 * u_NearDistance) 
		/ (u_FarDistance + u_NearDistance - d * (u_FarDistance - u_NearDistance));
}


float nonWorking(float aDepth)
{
	float ndc = 2 * aDepth - 1.0;

	float near = u_NearDistance;
	float far = u_FarDistance;

	return (2.0 * near * far) / (far + near - ndc * (far - near));
}

float my(float aDepth)
{
	float d = 2 * aDepth - 1.0;
	float n = u_NearDistance;
	float f = u_FarDistance;

	//return - (f - n) / (-2 * d * f * n + f + n);
	return (f - n) / (-2 * d * f * n + f + n);
}


float my_b(float aDepth)
{
	float d = 2 * aDepth - 1.0;
	//float d = aDepth;
	float n = u_NearDistance;
	float f = u_FarDistance;

	//return (f - n) / (-2 * d * f * n + f + n);
	return (f - n) / (2 * d * f * n + f + n);
}

////

void main(void)
{
	vec2 uv = ex_Uv;
	switch(u_Mode)
	{
		case MODE_DIRECTION:
			uv = gl_FragCoord.xy / textureSize(u_Texture, 0);
			break;
	}

	vec4 value = texture(u_Texture, uv);

	switch(u_Mode)
	{
		case MODE_LINEARIZE_DEPTH:
			out_Color = vec4(vec3(linearizeDepth(value.r)), 1);
			//out_Color = vec4(vec3(nonWorking(depthValue)), 1);
			//out_Color = vec4(vec3(my_b(depthValue)), 1);
			break;
		case MODE_DIRECTION:
			out_Color = vec4(mapToRgb(value.rgb), 1);
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
