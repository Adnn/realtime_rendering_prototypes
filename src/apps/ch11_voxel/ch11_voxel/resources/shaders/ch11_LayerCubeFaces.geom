#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

#define FACES 6

// One invocation per cube face
layout(invocations = FACES) in;

// TODO: address binding number
layout(std140, binding=14) uniform CubeFacesViewProjectionBlock
{
	mat4 ub_ViewingProjection[FACES];
    vec4 ub_LightPosition_world;
};

in vec3 ex_Position_world[]; // Position input, from the vertex shader

out vec3 ex_LightToVertex_world;


void main()
{
    for(uint vertexIdx = 0; vertexIdx !=3; ++vertexIdx)
    {
        // We attached a layered image (the cubemap level 0) to the framebuffer
        gl_Layer = gl_InvocationID;
        gl_Position =  
            ub_ViewingProjection[gl_Layer] 
            * vec4(ex_Position_world[vertexIdx], 1);
         
        // IMPORTANT: fragment depth cannot be interpolated from vertices
        // But the vertex-light ray can
        // Note: this is actually the same result for the 6 invocations on a given vertex
        // We could have a dedicated optimized vertex stage (the transformation being a substraction)
        ex_LightToVertex_world = ex_Position_world[vertexIdx] - ub_LightPosition_world.xyz;

        EmitVertex();
    }
    EndPrimitive();
}
