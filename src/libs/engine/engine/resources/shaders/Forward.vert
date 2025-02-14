#version 460

layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Color;

out vec3 vPosition;
out vec3 vColor;

void main(void)
{
	vColor = in_Color;
	vPosition = in_Position;
}
