#version 460

#include "shaders/LightsBlock.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3 * MAX_SHADOW_LIGHTS) out;

#define FACES 6

// One invocation per cube face
layout(invocations = FACES) in;

// TODO: address binding number
layout(std140, binding=14) uniform CubeFacesViewProjectionBlock
{
	mat4 ub_OrientationProjection[FACES];
};

in vec3 ex_Position_world[]; // Position input, from the vertex shader

out vec3 ex_LightToVertex_world;


void main()
{
    const uint omniMaps = min(ub_PointCount, MAX_SHADOW_LIGHTS);
	int layerOffset = 0;

    for(uint pointIdx = 0; pointIdx < omniMaps; ++pointIdx)
    {
		PointLight point = ub_PointLights[pointIdx];

		for(uint vertexIdx = 0; vertexIdx !=3; ++vertexIdx)
		{
			// We attached a layered image (the cubemap level 0) to the framebuffer
			gl_Layer = layerOffset + gl_InvocationID;

			// IMPORTANT: fragment depth cannot be interpolated from vertices
			// But the vertex-light ray can
			// Note: this is actually the same result for the 6 invocations on a given vertex
			// We could have a dedicated optimized vertex stage (the transformation being a substraction)
			ex_LightToVertex_world = ex_Position_world[vertexIdx] - point.position.xyz;

			gl_Position =  
				ub_OrientationProjection[gl_InvocationID] 
				* vec4(ex_LightToVertex_world, 1);
			 
			EmitVertex();
		}
		EndPrimitive();

		layerOffset += FACES;
	}
}
