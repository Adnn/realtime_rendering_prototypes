#version 460

#include "shaders/Helpers.glsl"


in vec2 ex_Uv;

#define DIRECTIONS

uniform sampler2D u_Texture;

uniform float u_NearDistance;
uniform float u_FarDistance;

out vec4 out_Color;


// Linearization functions 

float linearizeDepth(float aDepthBufferValue)
{
	return
		(2 * u_NearDistance) 
		/ (u_FarDistance + u_NearDistance - aDepthBufferValue * (u_FarDistance - u_NearDistance));
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
	#if defined(DIRECTIONS)
		vec2 uv = gl_FragCoord.xy / textureSize(u_Texture, 0);
	#else
		vec2 uv = ex_Uv;
	#endif

	vec4 value = texture(u_Texture, uv);

	#if defined(DIRECTIONS)
		out_Color = vec4(mapToRgb(value.rgb), 1);
	#else
		out_Color = vec4(vec3(linearizeDepth(value.r)), 1);
		//out_Color = vec4(vec3(nonWorking(depthValue)), 1);
		//out_Color = vec4(vec3(my_b(depthValue)), 1);
	#endif
}
