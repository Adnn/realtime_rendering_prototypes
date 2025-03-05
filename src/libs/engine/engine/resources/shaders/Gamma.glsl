uniform float u_Gamma = 2.2;


vec3 correctGamma(vec3 aColor)
{
    return pow(aColor, vec3(1./u_Gamma));
}


vec4 correctGamma(vec4 aColor)
{
    // Gamma compression from linear color space to "simple sRGB", as expected by the monitor.
    // (The monitor will do the gamma expansion.)
    return vec4(correctGamma(aColor.xyz), aColor.w);
}