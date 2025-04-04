// Compute the 2nd part of the split-sum approximation, "Environment BRDF",
// as described in "Real Shading in Unreal Engine 4"

#version 430


#include "IblUtilities.glsl"


in vec2 ex_Uv;

out layout(location = 2) vec2 out_Values;


void main()
{
    // We use the interpolated (u, v), in [0, 1]^2
    // as the linear interpolator for dot(n, v) and roughness.
    float alphaSquared = pow(alphaFromRoughness(ex_Uv.y), 2);
    float nDotV = ex_Uv.x; // i.e. cos(theta_v)

    out_Values = integrateBRDF(alphaSquared, nDotV);
}
