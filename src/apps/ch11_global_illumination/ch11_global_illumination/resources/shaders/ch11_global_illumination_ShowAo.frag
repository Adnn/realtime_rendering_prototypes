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
uniform ivec2 u_FramebufferSize;

out vec4 out_Color;

float e = 0.01;
vec2 o[16] = vec2[](
	vec2(-e, 0),
	vec2(+e, 0),
	vec2(0, -e),
	vec2(0, +e),
	vec2(e, -e),
	vec2(e, +e),
	vec2(-e, -e),
	vec2(-e, +e),
	vec2(-e * 2, 0),
	vec2(+e * 2, 0),
	vec2(0, -e * 2),
	vec2(0, +e * 2),
	vec2(e, -e * 2),
	vec2(e * 2, +e),
	vec2(-e * 2, -e),
	vec2(-e * 2, +e)
);

void main(void)
{
	const vec2 frag_uv = gl_FragCoord.xy / u_FramebufferSize;

	float accu = 0;
	for(uint i = 0; i != 16; ++i)
	{
		vec2 uv = frag_uv + o[i];
		accu += texture(u_DepthMap, vec3(uv, gl_FragCoord.z));
	}
	accu /= 16;


    out_Color = correctGamma(vec4(vec3(accu), 1));
}
