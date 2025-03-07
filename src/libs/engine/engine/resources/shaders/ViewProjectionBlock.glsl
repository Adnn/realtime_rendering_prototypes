#if !defined(VIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD)
#define VIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD


layout(std140, binding=0) uniform ViewProjectionBlock
{
	mat4 worldToCamera;
	mat4 cameraToWorld;
	mat4 projection;
	mat4 viewingProjection;
	vec4 cameraPosition_world;
};


#endif //VIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD
