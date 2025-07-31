#version 420


layout(location = 0) out vec4 out_Color;

#if defined(LINEARIZE_DEPTH)
	#include "shaders/Helpers.glsl"

	in vec3 ex_LightToVertex_world;

	uniform float u_NearDistance;
	uniform float u_FarDistance;
#endif


void main(void)
{
    // no requirement to explicitly write to gl_FragDepth, depth buffer is updated automatically.

	#if defined(LINEARIZE_DEPTH)
		gl_FragDepth = mapRangeToUnit(length(ex_LightToVertex_world), u_NearDistance, u_FarDistance);
	#endif
}