#version 460

#include "Gamma.glsl"
#include "Helpers.glsl"

in vec3 ex_Color;
in vec3 ex_Normal;
in vec3 ex_Position;

out vec4 out_Color;

void main(void)
{
	vec3 ambient_light = vec3(0.1, 0.1, 0.1);
	vec3 diffuse_light = vec3(0.5, 0.5, 0.5);
	vec3 specular_light = vec3(0.5, 0.5, 0.5);
	float specularExp = 20;

	vec3 normal_view = normalize(ex_Normal);
	vec3 lightDir_view = normalize(vec3(0.5, 0.0, 0.5));
	vec3 viewDir_view = normalize(-ex_Position);

	float diffuse = dotPlus(normal_view, lightDir_view);

	vec3 h = normalize(lightDir_view + viewDir_view);
	float specular = pow(dotPlus(normal_view, h), specularExp);

	vec3 color = ex_Color * (ambient_light + diffuse * diffuse_light + specular * specular_light);
	out_Color = correctGamma(vec4(color, 1.0));
}
