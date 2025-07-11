#version 460

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/LightsBlock.glsl"
#include "shaders/ViewProjectionBlock.glsl"


in vec3 ex_Position_world;

out vec4 out_Color;

uniform samplerCube u_CubeMap;

uniform float u_NearDistance;
uniform float u_FarDistance;


void main(void)
{

    // Point lights
    for(uint pointIdx = 0; pointIdx != ub_PointCount.x; ++pointIdx)
    {
        PointLight point = ub_PointLights[pointIdx];

        vec4 light_world = ub_cameraToWorld * vec4(point.position.xyz, 1);
		vec3 samplingDir = ex_Position_world - light_world.xyz;

		vec4 value =  texture(u_CubeMap, worldToCubemap(samplingDir.xyz));
        float result = value.r;

		out_Color = correctGamma(vec4(vec3(result), 1));
        return;
    }

}
