#version 460


#include "ViewProjectionBlock.glsl"


layout(location=0) in vec3 ve_Position;
layout(location=1) in vec3 ve_Normal;
layout(location=2) in vec4 ve_Color;
layout(location=3) in vec3 ve_Tangent;
layout(location=4) in vec3 ve_Bitangent;
layout(location=5) in vec2 ve_Uv01;

#if defined(ENTITIES)
	// Note: currently replaced by gl_InstanceID because we have 1:1 mapping between instances and entities
	// The instance attribute associating the OpenGL instance
	// (in the sense of instanced rendering) to its corresponding Entity.
	//layout(location=1) in uint in_EntityIdx; // Note: cannot be part of the include, which might be used in other shader stages
	#define ENTITY_IDX_ATTRIBUTE (gl_InstanceID + gl_BaseInstance)
	#include "EntitiesBlock.glsl"
#else
	// Hopefully optimized away
	float getModelTransform()
	{ return 1; }
#endif // ENTITIES


// Output interpolated for fragment shader
out vec4 ex_Color;
out vec3 ex_Position_world;
out vec3 ex_Normal_world;
out vec3 ex_Position_view;
out vec3 ex_Normal_view;
out vec3 ex_Tangent_view;
out vec3 ex_Bitangent_view;
out vec2 ex_Uv01;


void main(void)
{
	ex_Color = vec4(1)
		#if defined(ENTITIES)
			* getEntity().colorFactor
		#endif
		#if defined(VERTEX_COLOR)
			* ve_Color
		#endif
		;

	// TODO: handle non-uniform scaling with dedicated normal transform
	vec4 normal_world = getModelTransform() * vec4(ve_Normal, 0);
	ex_Normal_world = normal_world.xyz;
	ex_Normal_view = vec3(ub_worldToCamera * normal_world);

    // Tangents are transformed like positions, by the localToCamera matrix (unlike normals)
    // see: https://www.pbr-book.org/3ed-2018/Geometry_and_Transformations/Applying_Transformations
    mat4 localToCamera = ub_worldToCamera * getModelTransform();
    ex_Tangent_view   = normalize(mat3(localToCamera) * ve_Tangent);
    ex_Bitangent_view = normalize(mat3(localToCamera) * ve_Bitangent);

	vec4 position_world = getModelTransform() * vec4(ve_Position, 1.);
	ex_Position_world = position_world.xyz;
	vec4 position_view = ub_worldToCamera * position_world;
	ex_Position_view = position_view.xyz;

	ex_Uv01 = ve_Uv01;

	gl_Position = ub_projection * position_view;
}
