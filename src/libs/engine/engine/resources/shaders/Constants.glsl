#if !defined(CONSTANTS_GLSL_INCLUDE_GUARD)
#define CONSTANTS_GLSL_INCLUDE_GUARD


#define INVALID_INDEX uint(-1)

////
// Define values from client
////

// Note: we could use the defines provided by the client directly in the shader code
// but I would rather have the definition visible in some GLSL code to be grep friendly.
#define MAX_LIGHTS CLIENT_MAX_LIGHTS
#define MAX_MATERIALS CLIENT_MAX_MATERIALS

#endif // include guard