#version 460

in vec2 ex_Uv;

uniform sampler2D u_Texture;

uniform float u_NearDistance;
uniform float u_FarDistance;

out vec4 out_Color;


float linearizeDepth(float aDepthBufferValue)
{
	return
		(2 * u_NearDistance) 
		/ (u_FarDistance + u_NearDistance - aDepthBufferValue * (u_FarDistance - u_NearDistance));
}


void main(void)
{
	float depthValue = texture(u_Texture, ex_Uv).r;
	out_Color = vec4(vec3(linearizeDepth(depthValue)), 1);
}
