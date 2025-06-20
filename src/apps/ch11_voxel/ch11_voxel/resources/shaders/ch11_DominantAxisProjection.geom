#version 460

#include "ch11_VoxelsSsbo.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 ex_Position_world[];
in vec4 gi_Color[];
in vec2 gi_Uv01[];
in vec3 gi_Normal_view[];

out vec3 ex_Position_grid;

// usefull for a debug camera fragment shader
out vec3 ex_Position_view;
out vec4 ex_Color;
out vec3 ex_Normal_view;
out vec3 ex_Normal_world;
out vec2 ex_Uv01;

uniform vec3 u_CameraOffset;
uniform vec3 u_CameraScale;


void main(void)
{
	for(uint idx = 0; idx != 3; ++idx)
	{
		vec3 triangleNormal = 
			cross(ex_Position_world[1] - ex_Position_world[0],
				  ex_Position_world[2] - ex_Position_world[0]);

		// Position the camera in the center of the voxelized volume
		vec3 position_view = ex_Position_world[idx] + u_CameraOffset;

		// Map the position to the voxel grid
		ex_Position_grid = position_view * u_CameraScale;
		ex_Position_grid.xy = (ex_Position_grid.xy + 1) / 2 * ub_GridDimension;
		ex_Position_grid.z = (1 - (ex_Position_grid.z + 1) / 2) * ub_GridDimension;

		// Point the camera down the dominant axis

		// X dominant
		vec3 absNormal = abs(triangleNormal);
		if(absNormal.x > absNormal.y && absNormal.x > absNormal.z)
		{
			position_view = vec3(-position_view.z,
								 +position_view.y,
								 +position_view.x);
			//ex_Color = vec4(1.0, 0.0, 0.0, 1);
		}
		// Y dominant
		else if(absNormal.y > absNormal.z)
		{
			position_view = vec3(+position_view.x,
								 -position_view.z,
								 +position_view.y);
			//ex_Color = vec4(0.0, 1.0, 0.0, 1);
		}
		// Otherwise, Z dominant, which is already correctly aligned
		else
		{
			//ex_Color = vec4(0, 0, 1, 1);
		}
		ex_Color = gi_Color[idx];

		// With the dominant axis along Z, scale the voxelized volume to clip space
		gl_Position = vec4(position_view * u_CameraScale, 1);
		ex_Position_view = position_view.xyz;

		ex_Normal_world = normalize(triangleNormal);
		ex_Normal_view = gi_Normal_view[idx];

		// TODO: this is non-sense, it should forward the actual value
		// but we have a naming clash
		ex_Uv01 = gi_Uv01[idx];

		EmitVertex();
	}
	// Is it required to call EndPrimitive when we only emit one?
	// take the safe approach
	EndPrimitive();
}
