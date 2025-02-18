uniform float u_Gamma = 2.2;

vec4 correctGamma(vec4 aColor)
{
    // Gamma compression from linear color space to "simple sRGB", as expected by the monitor.
    // (The monitor will do the gamma expansion.)
    return vec4(pow(aColor.xyz, vec3(1./u_Gamma)), aColor.w);
}