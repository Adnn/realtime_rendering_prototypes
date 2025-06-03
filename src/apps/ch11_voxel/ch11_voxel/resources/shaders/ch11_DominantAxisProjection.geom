#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 ex_Position_world[];

out vec3 ex_Position_view;
// usefull for a debug camera fragment shader
out vec4 ex_Color;
out vec3 ex_Normal_view;

uniform vec3 u_CameraOffset;
uniform vec3 u_CameraScale;


void main(void)
{
	for(uint idx = 0; idx != 3; ++idx)
	{
		vec3 position_view = ex_Position_world[idx] + u_CameraOffset;
		gl_Position = vec4(position_view * u_CameraScale, 1);
		ex_Position_view = position_view.xyz;

		ex_Color = vec4(1);
		ex_Normal_view = normalize(
			cross(ex_Position_world[1] - ex_Position_world[0],
				  ex_Position_world[2] - ex_Position_world[0]));

		EmitVertex();
	}
	// Is it required to call EndPrimitive when we only emit one?
	// take the safe approach
	EndPrimitive();
}
