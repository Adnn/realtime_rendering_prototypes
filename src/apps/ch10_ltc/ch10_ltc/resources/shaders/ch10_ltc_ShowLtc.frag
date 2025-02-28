#version 460

#define PI 3.1415926535897932384626433832795

in vec4 ex_Color;
in vec3 ex_Normal_world;
in vec3 ex_Position_world;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;

out vec4 out_Color;


/// @param aDir must be normalized
float clampedCos(vec3 aDir)
{
    return max(0, aDir.z) / PI;
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
    uint mode = 3;
    mat3 M = mat3(1);

    switch (mode)
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
            break;
    }

    mat3 M_inv = inverse(M);

    vec3 omega = N;
    vec3 omega_0_scaled = M_inv * omega;
    float norm = length(omega_0_scaled);
    vec3 omega_0 = omega_0_scaled / norm;
    float jacobian = determinant(M_inv) / pow(norm, 3);

    // Without the jacobian factor
    //vec3 fragmentColor = vec3(clampedCos(normalize(M_inv * omega)));

    // 3.1 Closed-Form Expression:
    vec3 fragmentColor = vec3(clampedCos(omega_0) * jacobian);

    out_Color = vec4(fragmentColor, 1);
}
