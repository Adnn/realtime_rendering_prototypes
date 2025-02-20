#version 460


layout(location=0) in vec3 in_Position;
// The instance attribute associating the OpenGL instance
// (in the sense of instanced rendering) to its corresponding Entity.
layout(location=1) in uint in_EntityIdx; // Note: cannot be part of the include, which might be used in other shader stages

#define ENTITY_IDX_ATTRIBUTE in_EntityIdx
#include "EntitiesBlock.glsl"

out vec4 vColor;
out flat uint vEntityIdx;
out vec3 vPosition;


void main(void)
{
	vColor = getEntity().colorFactor;
	vEntityIdx = in_EntityIdx;
	vPosition = in_Position;
}
