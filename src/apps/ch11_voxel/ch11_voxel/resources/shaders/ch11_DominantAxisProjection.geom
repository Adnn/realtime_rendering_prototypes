#version 460


// TODO: replace with our custom orthogonal projection
#include "shaders/ViewProjectionBlock.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 ex_Position_world[];

out vec3 ex_Position_view;

uniform vec3 u_CameraOffset;
uniform float u_CameraScale;


void main(void)
{
	for(uint idx = 0; idx != 3; ++idx)
	{
		vec4 position_view = ub_worldToCamera * vec4(ex_Position_world[idx], 1);
		gl_Position = ub_projection * position_view;
		ex_Position_view = position_view.xyz;
		EmitVertex();
	}
	// Is it required to call EndPrimitive when we only emit one?
	// take the safe approach
	EndPrimitive();
}
