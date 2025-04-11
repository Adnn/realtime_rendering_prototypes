#version 460


layout(location=0) in vec3 in_Position;

// Note: currently replaced by gl_InstanceID because we have 1:1 mapping between instances and entities
// The instance attribute associating the OpenGL instance
// (in the sense of instanced rendering) to its corresponding Entity.
//layout(location=1) in uint in_EntityIdx; // Note: cannot be part of the include, which might be used in other shader stages

#define ENTITY_IDX_ATTRIBUTE (gl_InstanceID + gl_BaseInstance)
#include "EntitiesBlock.glsl"

out vec4 vColor;
out flat uint vEntityIdx;
out vec3 vPosition;


void main(void)
{
	vColor = getEntity().colorFactor;
	vEntityIdx = ENTITY_IDX_ATTRIBUTE;
	vPosition = in_Position;
}
