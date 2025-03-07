#if !defined(VIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD)
#define VIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD


layout(std140, binding=0) uniform ViewProjectionBlock
{
	mat4 ub_worldToCamera;
	mat4 ub_cameraToWorld;
	mat4 ub_projection;
	mat4 ub_viewingProjection;
	vec4 ub_cameraPosition_world;
};


#endif //VIEWPROJECTIONBLOCK_GLSL_INCLUDE_GUARD
