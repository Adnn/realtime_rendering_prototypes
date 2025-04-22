#version 460

#define PI 3.1415926535897932384626433832795

in vec4 ex_Color;
in vec3 ex_Normal_world;
in vec3 ex_Position_world;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;

uniform sampler1D u_LtcColorMap;
uniform uint u_Letter = 2;

out vec4 out_Color;


/// @param aDir must be normalized
float clampedCos(vec3 aDir)
{
    return max(0, aDir.z) / PI;
}

/// @param aDir must be normalized
float evaluateLtc(vec3 aDir, mat3 M_inv)
{
    vec3 omega = aDir;
    vec3 omega_0_scaled = M_inv * omega;
    float norm = length(omega_0_scaled);
    vec3 omega_0 = omega_0_scaled / norm;
    float jacobian = determinant(M_inv) / pow(norm, 3);

    return clampedCos(omega_0) * jacobian;
}


// See: "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines"
// https://drive.google.com/file/d/0BzvWIdpUpRx_d09ndGVjNVJzZjA/view?resourcekey=0-21tmiqk55JIZU8UoeJatXQ
void main(void)
{
    vec3 shadingNormal_world = normalize(ex_Normal_world);
    vec3 shadingNormal_view = normalize(ex_Normal_view);
    vec3 N = shadingNormal_world;

    // GLSL matrices are column major (so, the first 3 values are the first column)

    // Mode 0 to 3 maps to domains a to d in Figure 2.
    mat3 M = mat3(1);
    vec3 maxDir = vec3(0, 0, 1);

    switch (u_Letter)
    {
        case 1:
            M = mat3(0.3, 0,   0,
                     0,   0.3, 0,
                     0,   0,   1);
            break;
        case 2:
            M = mat3(0.8, 0,   0,
                     0,   0.2, 0,
                     0,   0,   1);
            break;
        case 3:
            M = mat3(1, 0, 1,
                     0, 1, 0,
                     0, 0, 1);
            // Approximation, found by rough manual dichotomy
			maxDir = normalize(vec3(0.45, 0, 1));
            break;
    }

    mat3 M_inv = inverse(M);

    #define JACOBIAN
    #if ! defined(JACOBIAN)
		// Without the jacobian factor
		float value = clampedCos(normalize(M_inv * omega));
    #else
		// 3.1 Closed-Form Expression:
		float value = evaluateLtc(N, M_inv);
		float max = evaluateLtc(maxDir, M_inv);
        // normalize for display, as done by the plot in original code
        // see: https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/fit/plot.h#L140
        value /= max;
    #endif

    #define COLOR_MAPPING
    #if ! defined(COLOR_MAPPING)
		vec3 fragmentColor = vec3(value);
    #else
		int colorMapWidth = textureSize(u_LtcColorMap, 0);
		// I think this is the correct texture u coordinate, to to start interpolation at (0 + epsilon)
		// and keep it going until (1 - epsilon). It does not make an observable difference though.
		float u = value * (colorMapWidth - 1) / colorMapWidth + (1 / 2 * colorMapWidth);
		vec3 fragmentColor = texture(u_LtcColorMap, value).xyz;
	#endif // COLOR_MAPPING

    out_Color = vec4(fragmentColor, 1);
}
