#version 460


#include "ch11_VoxelConeTracing.glsl"
#include "ch11_VoxelsUtilities.glsl"

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/MaterialGenericBlock.glsl"
#include "shaders/ViewProjectionBlock.glsl"


in vec4 ex_Color;
in vec3 ex_Normal_world;
in vec3 ex_Position_world;
in vec2 ex_Uv01;

uniform uint u_MaterialIdx;
uniform uint u_MraoUvChannel;
uniform uint u_DiffuseUvChannel;
uniform sampler2D u_MraoTexture;
uniform sampler2D u_DiffuseTexture;

uniform float u_VoxelSize;
uniform vec3 u_AabbMin;

uniform uint u_ConeTraceMode;

out vec4 out_Color;


void main(void)
{
    const uint gNoTextureChannel = uint(-1);

    //MaterialGeneric material = ub_MaterialGeneric[u_MaterialIdx];

    vec4 albedo = ex_Color;
    if(u_DiffuseUvChannel != gNoTextureChannel)
    {
        albedo *= texture(u_DiffuseTexture, ex_Uv01);
	}

    //
    // alpha testing for cutout
    //
    if (albedo.a < 0.5)
    {
        discard;
    }

	float metallic = 0.0;
	float roughness = 0.5;
	if(u_MraoUvChannel != gNoTextureChannel)
	{
		vec4 mrao = texture(u_MraoTexture, ex_Uv01);
		// glTF sponza channel order
		metallic = mrao.b;
		roughness = mrao.g;
	}

	vec3 position_aabb = ex_Position_world - u_AabbMin;
	vec3 normal_world = normalize(ex_Normal_world);

	//
	// Diffuse
	//
	vec4 diffuseIrradiance = accumulateDiffuseIndirect(position_aabb,
												  	   normal_world,
												  	   u_TanHalfAperture,
												  	   u_VoxelSize);

	//
	// Specular
	//
	vec3 incident_world = normalize(ex_Position_world - getCameraPosition_world());
	vec4 specularIrradiance = accumulateSpecularIndirect(position_aabb,
													     normal_world,
														 incident_world,
														 roughness,
														 u_VoxelSize);

	switch(u_ConeTraceMode)
	{
		case CLIENT_CONETRACE_AO:
			out_Color = correctGamma(vec4(vec3(1-diffuseIrradiance.a), 1));
			break;
		case CLIENT_CONETRACE_DIFFUSE:
			out_Color = correctGamma(vec4(diffuseIrradiance.rgb, 1));
			break;
		case CLIENT_CONETRACE_SPECULAR:
			out_Color = correctGamma(vec4(specularIrradiance.rgb, 1));
			break;
	}
}
