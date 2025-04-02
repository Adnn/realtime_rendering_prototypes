#version 420

#include "Cube.glsl"
#include "ViewProjectionBlock.glsl"

out vec3 ex_FragmentPosition_world;

void main(void)
{
    vec3 position_local = gCubePositions[gCubeIndices_triangleStrip[gl_VertexID]];

    // Note: We do not want to apply camera translation to the skybox vertices,
    // so we discard translation part by restricting worlToCamera to a mat3
    vec4 pos = 
        ub_projection
        * mat4(mat3(ub_worldToCamera))
        * vec4(position_local, 1.0)
        ;
    // Because we later discard the z component (mulitplication by projection impacts z when w=1), 
    // this is equivalent to:
    //vec4 pos = ub_viewingProjection * vec4(position_local, 0.0);

    // Optimization: Setting z component to w ensures the fragment will be at maxdepth
    // i.e. it will be early discarded if another fragment was written
    gl_Position = pos.xyww;

    // The interpolated world coordinate of the fragment will be the direction to sample in cubemap
    ex_FragmentPosition_world = position_local;
}