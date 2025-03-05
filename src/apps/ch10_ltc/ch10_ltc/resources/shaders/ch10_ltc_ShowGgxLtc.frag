#version 460

#define PI 3.1415926535897932384626433832795

in vec4 ex_Color;
in vec3 ex_Normal_world;
in vec3 ex_Position_world;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;

uniform sampler1D u_LtcColorMap;
uniform sampler2D u_Ltc_1;
uniform sampler2D u_Ltc_2;

uniform float u_alpha = 0.3;
uniform float u_thetaViewDir = 45 * PI/180; // Radians

// TODO: The size should be fetched from the actual texture dimension
const float LUT_SIZE  = 64.0;
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;

out vec4 out_Color;


/// @param aDir must be normalized
float clampedCos(vec3 aDir)
{
    return max(0, aDir.z) / PI;
}


// @param aDir must be normalized
float evaluateLtc(vec3 aDir, mat3 M_inv)
{
    vec3 omega = aDir;
    vec3 omega_0_scaled = M_inv * omega;
    float norm = length(omega_0_scaled);
    vec3 omega_0 = omega_0_scaled / norm;
    float jacobian = determinant(M_inv) / pow(norm, 3);

    return clampedCos(omega_0) * jacobian;
}

// The implementation found in Stephen Hill repository
// see: https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/fit/LTC.h#L51
float evaluateLtc_Hill(vec3 aDir, mat3 M_inv)
{
    mat3 M = inverse(M_inv);

    vec3 omega = aDir;
    vec3 omega_0 = normalize(M_inv * omega);
    vec3 omega_0_scaled = M * omega_0;
    float norm = length(omega_0_scaled);
    float jacobian = abs(determinant(M)) / pow(norm, 3);

    return clampedCos(omega_0) / jacobian;
}

#define EVALUATE evaluateLtc

// see: https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/fit/LTC.h#L65-L71
vec3 sampleDirection(float U1, float U2, mat3 M)
{
	float theta = acos(sqrt(U1));
	float phi = 2.0f*3.14159f * U2;
	vec3 L = normalize(M * vec3(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta)));
	return L;
}


float sampleMaxValue(mat3 M_inv)
{
    const mat3 M = inverse(M_inv);
	float max_value = 0.0;

	const int Nsample = 50;
	for (int j = 0; j < Nsample; ++j)
    {
		for (int i = 0; i < Nsample; ++i)
		{
			const float U1 = (i + 0.5f)/Nsample;
			const float U2 = (j + 0.5f)/Nsample;
			max_value = max(max_value, EVALUATE(sampleDirection(U1, U2, M), M_inv));
		}
	}
	return max_value;
}


// See: "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines"
// https://drive.google.com/file/d/0BzvWIdpUpRx_d09ndGVjNVJzZjA/view?resourcekey=0-21tmiqk55JIZU8UoeJatXQ
void main(void)
{
    vec3 shadingNormal_world = normalize(ex_Normal_world);
    vec3 N = shadingNormal_world;

    // Draw the equator line
    if (abs(N.z) < 0.004)
    {
        out_Color = vec4(vec3(0), 1);
        return;
    }

    // the view direction is given as the angle from normal
    // No need to clamp, as negative values will sample border texture value
    float nDotV = cos(u_thetaViewDir);
    
	// TODO: We have to clarify wether the texture is parameterized on roughness
    // or on alpha (which is usually roughness^2)
    // From "Representation and Storage", it shoud be parameterized on sqrt(alpha)
    // that we take to mean roughness, which is consistent with the fetch in sample:
    // https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/webgl/shaders/ltc/ltc_quad.fs#L433

    // Note: the sample code use a sqrt, which does not seem to be explained by the paper
    vec2 uv = vec2(sqrt(u_alpha), sqrt(1.0 - nDotV));
    uv = uv * LUT_SCALE + LUT_BIAS;

    vec4 t1 = texture(u_Ltc_1, uv);
    vec4 t2 = texture(u_Ltc_2, uv);

    // Note: GLSL matrix are column major (so each vec3 below is a column)
    // Note that this simply matches the order in which the glm::mat3 (also column major)
    // is packed into the texture:
    // https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/fit/fitLTC.cpp#L356-L359
    mat3 M_inv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(  0,  1,    0),
        vec3(t1.z, 0, t1.w)
    );

	// 3.1 Closed-Form Expression:
	float value = EVALUATE(N, M_inv);

    // Sample the hemisphere looking for an (approximate) maximum
	float max = sampleMaxValue(M_inv);
	value /= max;

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
