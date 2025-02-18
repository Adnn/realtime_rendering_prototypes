#version 460

#include "Gamma.glsl"
#include "Helpers.glsl"

in vec3 ex_Color;
in vec3 ex_Normal;
in vec3 ex_Position;

out vec4 out_Color;

uniform vec3 u_lightDir_view;

void main(void)
{
	vec3 ambient_light = vec3(0.1, 0.1, 0.1);
	vec3 diffuse_light = vec3(0.5, 0.5, 0.5);
	vec3 specular_light = vec3(0.5, 0.5, 0.5);
	float specularExp = 20;

	vec3 normal_view = normalize(ex_Normal);
	vec3 viewDir_view = normalize(-ex_Position);

	float nDotL = dotPlus(normal_view, u_lightDir_view);
	float diffuse = nDotL;

    // Eliminate specular light bleeding, with a boolean multiplicative factor
    // see: https://computergraphics.stackexchange.com/q/14072/11110
    // Note: this has a major drawback: it introduces an abrupt cut-off between polygons
    // where the sign of nDotL changes, which is very disturbing for smooth surfaces.
    // Use a smoothstep windowing function instead of boolean value, to avoid discontinuity.
	float isFacing = smoothstep(0., 0.2, nDotL);

	vec3 h = normalize(u_lightDir_view + viewDir_view);
	float specular = isFacing * pow(dotPlus(normal_view, h), specularExp);

	vec3 color = ex_Color * (ambient_light + diffuse * diffuse_light + specular * specular_light);
	out_Color = correctGamma(vec4(color, 1.0));
}
