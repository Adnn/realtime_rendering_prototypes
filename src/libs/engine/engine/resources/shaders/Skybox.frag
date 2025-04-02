#version 420

#include "Gamma.glsl"
#include "Helpers.glsl"

in vec3 ex_FragmentPosition_world;

#if defined(EQUIRECTANGULAR)
uniform sampler2D u_EnvironmentTexture;
#else
uniform samplerCube u_EnvironmentTexture;
#endif

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec3 out_LinearHdr;


void main()
{
#if defined(EQUIRECTANGULAR)
    // For the conversion procedure, see: rtr 4th p407
    // (the book does metion +z is up, but does not define the complete basis.
    // I suppose it is the physic basis from: 
    // https://en.wikipedia.org/wiki/Spherical_coordinate_system,
    // so x becomes y, y becomes z, z becomes x).
    vec3 view_world = normalize(ex_FragmentPosition_world);

    // This formula show the middle of the equirectangle with default camera looking down -Z (in world).
    // The value is mirrored on the range [0, 1]:
    // an increasing azimuth (counterclockwise) has to lead to a decreasing u to avoid mirroring.
    float u = 1 - atan(view_world.x, view_world.z) / (2 * M_PI);
    // Which give the same result as:
    //float u = atan(-view_world.x, view_world.z) / (2 * M_PI);

    // Polar angle increase in the opposite direction compared to v coordinate
    float v = 1 - acos(view_world.y) / M_PI;

    vec3 envColor = texture(u_EnvironmentTexture, vec2(u,v)).rgb;
#else
    // Cubemap coordinate system is left handed, but the cube texture coords is given
    // in right handed world space, so negate Z. 
    // Note: the individual images in the cubemap texture are loaded "upside-down" compared to usual OpenGL textures
    // in order for this to work (you can see they are upside down in Nsight Graphics)
    vec3 envColor = texture(u_EnvironmentTexture, worldToCubemap(ex_FragmentPosition_world)).rgb;
#endif
    out_Color = correctGamma(vec4(envColor, 1.0));
    out_LinearHdr = envColor;
}
