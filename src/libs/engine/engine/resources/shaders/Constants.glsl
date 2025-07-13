#if !defined(CONSTANTS_GLSL_INCLUDE_GUARD)
#define CONSTANTS_GLSL_INCLUDE_GUARD


const float M_PI = 3.141592653589793;

#define INVALID_INDEX uint(-1)

////
// Define values from client
////

// Note: we could use the defines provided by the client directly in the shader code
// but I would rather have the definition visible in some GLSL code to be grep friendly.
#define MAX_ENTITIES CLIENT_MAX_ENTITIES
#define MAX_LIGHTS CLIENT_MAX_LIGHTS
#define MAX_SHADOW_LIGHTS CLIENT_MAX_SHADOW_LIGHTS
#define MAX_SHADOW_MAPS CLIENT_MAX_SHADOW_MAPS
#define MAX_MATERIALS CLIENT_MAX_MATERIALS
#define SSAO_SAMPLE_COUNT CLIENT_SSAO_SAMPLE_COUNT

#endif // include guard