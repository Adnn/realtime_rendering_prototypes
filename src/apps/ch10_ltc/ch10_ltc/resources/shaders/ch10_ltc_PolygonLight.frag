#version 460

#include "ch10_ltc_LightsBlock.glsl"

#include "shaders/Constants.glsl"
#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/MaterialPbrBlock.glsl"
#include "shaders/PbrUtilities.glsl"
#include "shaders/ViewProjectionBlock.glsl"

in vec4 ex_Color;
in vec3 ex_Normal_world;
in vec3 ex_Position_world;
in vec3 ex_Normal_view;
in vec3 ex_Position_view;

uniform sampler2D u_Ltc_1;
uniform sampler2D u_Ltc_2;

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


// see: https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/webgl/shaders/ltc/ltc_quad.fs#L143
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}


// TODO: handle arbitrary N polygons
// see: https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/webgl/shaders/ltc/ltc_quad.fs#L274
vec3 integrateLtcOverPolygon(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided)
{
    // Construct the TBN basis, around N, and orienting tagent along V
    vec3 T1, T2;
    // TODO: make robust to parallel N and V
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);


    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    // integrate
    float sum = 0.0;

    bool clipless = true;

    // TODO implement clipping
    if (clipless)
    {
        vec3 dir = points[0].xyz - P;
        vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
        bool behind = (dot(dir, lightNormal) < 0.0);

        L[0] = normalize(L[0]);
        L[1] = normalize(L[1]);
        L[2] = normalize(L[2]);
        L[3] = normalize(L[3]);

        vec3 vsum = vec3(0.0);

        vsum += IntegrateEdgeVec(L[0], L[1]);
        vsum += IntegrateEdgeVec(L[1], L[2]);
        vsum += IntegrateEdgeVec(L[2], L[3]);
        vsum += IntegrateEdgeVec(L[3], L[0]);

        float len = length(vsum);
        float z = vsum.z/len;

        if (behind)
            z = -z;

        vec2 uv = vec2(z*0.5 + 0.5, len);
        uv = uv*LUT_SCALE + LUT_BIAS;

        float scale = texture(u_Ltc_2, uv).w;

        sum = len*scale;

        if (behind && !twoSided)
            sum = 0.0;
    } 

    vec3 Lo_i = vec3(sum, sum, sum);
    return Lo_i;
}


// See: "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines"
// https://drive.google.com/file/d/0BzvWIdpUpRx_d09ndGVjNVJzZjA/view?resourcekey=0-21tmiqk55JIZU8UoeJatXQ
void main(void)
{
    MaterialPbr material = ub_MaterialPbr[0];

    vec4 albedo = ex_Color * material.baseColor;
    float metallic = material.metallicRoughness.x;
    float roughness = material.metallicRoughness.y;
    float alpha = alphaFromRoughness(roughness);
    alpha = max(0.005, alpha);

    //vec3 shadingNormal_view = normalize(ex_Normal_view);
    vec3 shadingNormal_world = normalize(ex_Normal_world);
    vec3 N = shadingNormal_world;

    //vec3 viewDir_view = normalize(-ex_Position_view);
    vec3 viewDir_world = normalize(ub_cameraPosition_world.xyz - ex_Position_world);
    vec3 view = viewDir_world;

    vec3 P = ex_Position_world;

    // No need to clamp, as negative values will sample border texture value
    float nDotV = dot(N, view);
    
	// TODO: We have to clarify wether the texture is parameterized on roughness
    // or on alpha (which is usually roughness^2)
    // From "Representation and Storage", it shoud be parameterized on sqrt(alpha)
    // that we take to mean roughness, which is consistent with the fetch in sample:
    // https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/webgl/shaders/ltc/ltc_quad.fs#L433

    // TODO: mirroring the texture on the Y axis would save the the substraction from one.
    vec2 uv = vec2(roughness, sqrt(1.0 - nDotV));
    uv = uv * LUT_SCALE + LUT_BIAS;

    vec4 t1 = texture(u_Ltc_1, uv);
    vec4 t2 = texture(u_Ltc_2, uv);
    // t2 components:
    // * x: average magnitude
    // * y: mean fresnel
    // * z: unused
    // * w: "projected (cosine-weighted) solid angle of spherical cap". 
    //      The reference implementation uses it to scale the result when the polygon 
    //      is not clipped against horizon

    // Note: GLSL matrix are column major (so each vec3 below is a column)
    // Note that this simply matches the order in which the glm::mat3 (also column major)
    // is packed into the texture:
    // https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/fit/fitLTC.cpp#L356-L359
    mat3 M_inv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(  0,  1,    0),
        vec3(t1.z, 0, t1.w)
    );


	vec3 spec;
	vec3 diff;

    for(uint planarIdx = 0; planarIdx != ub_PlanarCount; ++planarIdx)
    {
        CardLight light = ub_PlanarLights[planarIdx];
		vec3 points[4] = getPolygon(light);

		spec += integrateLtcOverPolygon(N, view, P, M_inv, points, light.doubleSided)
                * light.colors.specular.rgb;
		// Diffuse lambertian BRDF is exactly the untransformed clamped cosine.
		diff += integrateLtcOverPolygon(N, view, P, mat3(1), points, light.doubleSided)
                * light.colors.diffuse.rgb;
	}


    // We blend the parameters before computing the lighting model.
    // This is not physically correct (parameters do not have linear relationship to output)
    // but this is fast and results are still convincing. 
    PbrParameters pbrParameters;
    pbrParameters.diffuseColor = mix(albedo.rgb, vec3(0.), metallic);
    pbrParameters.f0 = mix(gF0_dielec, albedo.rgb, metallic);
    pbrParameters.f90 = gF90;
    pbrParameters.alpha = alpha;

//#define REFERENCE_ILLUMINATION
#if defined(REFERENCE_ILLUMINATION)
	//// BRDF shadowing and Fresnel
    vec3 scol = pbrParameters.f0;
	spec *= scol*t2.x + (1.0 - scol)*t2.y;
    diff *= pbrParameters.diffuseColor;
#else
    // TODO: how to properly understand t2.x?
    vec3 fresnel = pbrParameters.f0 /* * t2.x */ + (pbrParameters.f90 - pbrParameters.f0) * t2.y;
    spec *= fresnel;
    diff *= (1 - fresnel) * pbrParameters.diffuseColor;
#endif

    // Note: the ambient term is a quick hack, to be removed when IBL is in place
    // We multiply it by the diffuse color, so metals do not have ambient terms, and dielectrics have their tint.
    vec3 ambient =  ub_AmbientColor.rgb * material.ambientColor.rgb * pbrParameters.diffuseColor;

    vec3 fragmentColor = ambient + diff + spec;
    out_Color = vec4(correctGamma(fragmentColor), 1);
}
