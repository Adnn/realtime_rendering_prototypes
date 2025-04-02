#if !defined(CUBE_GLSL_INCLUDE_GUARD)
#define CUBE_GLSL_INCLUDE_GUARD


vec3 gCubePositions[8] = vec3[8](
    vec3(-1, -1, -1),
    vec3(-1, -1,  1),
    vec3(-1,  1, -1),
    vec3(-1,  1,  1),

    vec3( 1, -1, -1),
    vec3( 1, -1,  1),
    vec3( 1,  1, -1),
    vec3( 1,  1,  1)
);


uint gCubeIndices[36] = uint[36](
    // Left
    0, 1, 2,
    2, 1, 3,
    // Front
    1, 5, 3,
    3, 5, 7,
    // Right,
    5, 4, 7,
    7, 4, 6,
    // Back
    4, 0, 6,
    6, 0, 2,
    // Top
    6, 2, 7,
    7, 2, 3,
    // Bottom,
    0, 4, 1,
    1, 4, 5
);


// see: https://stackoverflow.com/a/38855946
uint gCubeIndices_triangleStrip[14] = uint[14](
    2, 6, 0, 4, 5, 6, 7, 2, 3, 0, 1, 5, 3, 7
);

#endif //CUBE_GLSL_INCLUDE_GUARD