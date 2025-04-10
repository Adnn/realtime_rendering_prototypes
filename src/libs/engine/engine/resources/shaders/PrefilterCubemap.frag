#version 430


#include "IblUtilities.glsl"
#include "PbrUtilities.glsl"


in vec3 ex_FragmentPosition_world;

#if defined(EQUIRECTANGULAR)
uniform sampler2D u_EnvironmentTexture;
#else
uniform samplerCube u_EnvironmentTexture;
#endif // EQUIRECTANGULAR

uniform float u_Roughness;

layout(location = 0) out vec4 out_LinearHdr;


void main()
{
    float alphaSquared = pow(alphaFromRoughness(u_Roughness), 2);
    out_LinearHdr = vec4(
#if defined(SPECULAR_RADIANCE)
        prefilterEnvMapSpecular(alphaSquared, normalize(ex_FragmentPosition_world), u_EnvironmentTexture)
#elif defined(DIFFUSE_IRRADIANCE)
        // Note: we use the F0 of dielectric, because metals do not have a diffuse contribution
        prefilterEnvMapDiffuse_LambertianFresnel(normalize(ex_FragmentPosition_world),
                                                 gF0_dielec,
                                                 u_EnvironmentTexture)
#endif
    , 1); // In case GL_BLEND is enabled, make it fully opaque.
}
