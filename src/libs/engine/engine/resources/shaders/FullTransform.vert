#version 460


#include "ViewProjectionBlock.glsl"


layout(location=0) in vec3 ve_Position;
layout(location=1) in vec3 ve_Normal;

// Note: currently replaced by gl_InstanceID because we have 1:1 mapping between instances and entities
// The instance attribute associating the OpenGL instance
// (in the sense of instanced rendering) to its corresponding Entity.
//layout(location=1) in uint in_EntityIdx; // Note: cannot be part of the include, which might be used in other shader stages
#define ENTITY_IDX_ATTRIBUTE (gl_InstanceID + gl_BaseInstance)
#include "EntitiesBlock.glsl"


// Output interpolated for fragment shader
out vec4 ex_Color;
out vec3 ex_Normal_world;
out vec3 ex_Position_world;
out vec3 ex_Normal_view;
out vec3 ex_Position_view;


void main(void)
{
	ex_Color = getEntity().colorFactor;

	// TODO: handle non-uniform scaling with dedicated normal transform
	vec4 normal_world = getModelTransform() * vec4(ve_Normal, 0);
	ex_Normal_world = normal_world.xyz;
	ex_Normal_view = vec3(ub_worldToCamera * normal_world);

	vec4 position_world = getModelTransform() * vec4(ve_Position, 1.);
	ex_Position_world = position_world.xyz;
	vec4 position_view = ub_worldToCamera * position_world;
	ex_Position_view = position_view.xyz;

	gl_Position = ub_projection * position_view;
}
