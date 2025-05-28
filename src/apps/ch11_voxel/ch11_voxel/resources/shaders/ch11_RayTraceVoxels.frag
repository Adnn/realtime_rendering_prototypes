#version 460


#include "shaders/ViewProjectionBlock.glsl"


in vec2 ex_Uv;

uniform uint u_GridSide;

layout(std430, binding = 10) buffer VoxelsSsbo
{
  uint ub_Voxels[];
};

out vec4 out_Color;


uniform vec3 u_AabbMin;
uniform vec3 u_AabbMax;
uniform ivec2 u_FramebufferSize;
// Size of the image plane at a distance 1 from the camera origin
uniform vec2 u_ImagePlane_view;

void main(void)
{
	vec2 fragPos_ndc = (gl_FragCoord.xy / u_FramebufferSize) * 2 - 1;
	// Important: the AABB is given in canonical world coordinates
	vec3 rayOrigin_world = getCameraPosition_world();
	// Note: we could cancel out the division by two from the multiplication in fragPos
	vec4 rayDir_view = vec4(fragPos_ndc * u_ImagePlane_view / 2, -1, 0);
	vec3 rayDir_world = (ub_cameraToWorld * rayDir_view).xyz;
	//rayDir_world = normalize(rayDir_world);
	
	// Time at slab 0 (the AABB origin)
	vec3 t_0 = (u_AabbMin - rayOrigin_world) / rayDir_world;
	// Time at slab 1 (AABB opposite corner)
	vec3 t_1 = (u_AabbMax - rayOrigin_world) / rayDir_world;

	vec3 tmin = min(t_0, t_1);
	vec3 tmax = max(t_0, t_1);

	float tIn = max(max(tmin.x, tmin.y), tmin.z);
	float tOut  = min(min(tmax.x, tmax.y), tmax.z);

	// TODO: handle clipping planes
	if (tIn <= tOut && tOut >= 0)
	{
		// Intersection found
		float t = max(0, tIn);
		
		if(tmin.x > tmin.y && tmin.x > tmin.z)
		{
			out_Color = vec4(vec3(0.75), 1);
			return;
		}
		else if(tmin.y > tmin.z)
		{
			out_Color = vec4(vec3(0.5), 1);
			return;
		}
		else
		{
			out_Color = vec4(vec3(0.25), 1);
			return;
		}
	}
	else
	{
		out_Color = vec4(0.3, 0, 0, 1);
	}

}
