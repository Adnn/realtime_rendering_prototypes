#version 460

#include "ch11_VoxelConeTracing.glsl"

#include "shaders/Gamma.glsl"
#include "shaders/Helpers.glsl"
#include "shaders/IblUtilities.glsl"
#include "shaders/LightsBlock.glsl"
#include "shaders/LightUtilities.glsl"
#include "shaders/MaterialGenericBlock.glsl"
#include "shaders/PbrUtilities.glsl"
#include "shaders/ToneMapping.glsl"
#include "shaders/ViewProjectionBlock.glsl"


// Also used for specular component of indrect light
uniform sampler2D u_IntegratedEnvironmentBrdf;
#if defined(ENVIRONMENT_MAPPING)
    uniform samplerCube u_EnvironmentTexture;
    uniform samplerCube u_FilteredRadianceEnvironmentTexture;
    uniform samplerCube u_FilteredIrradianceEnvironmentTexture;

    // Control the IBL contributions strenght, good candidates to be part of each environment
    uniform float u_SpecularIblFactor = 1.0;
    uniform float u_DiffuseIblFactor = 1.0;
#endif //ENVIRONMENT_MAPPING


#if defined(SHADOW_MAPPING)
    #include "ch11_Shadow.glsl"
#endif //SHADOW_MAPPING


in vec4 ex_Color;
in vec3 ex_Position_view;
in vec3 ex_Position_world;
in vec3 ex_Normal_view;
in vec3 ex_Tangent_view;
in vec3 ex_Bitangent_view;
in vec2 ex_Uv01;

out vec4 out_Color;

uniform sampler2D u_AmbientOcclusion;
uniform sampler2D u_DiffuseTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_MraoTexture;

uniform uint u_DiffuseUvChannel;
uniform uint u_NormalUvChannel;
uniform uint u_MraoUvChannel;

uniform uint u_MaterialIdx;

uniform bool u_SplitSumIndirectSpecular = true;
uniform bool u_ApplyAo = false;
uniform bool u_ApplyEnvironment;
uniform bool u_ApplyNormalMap = true;
uniform uint u_ToneMapping;
uniform uint u_ShadowMethod;

uniform ivec2 u_FramebufferSize;
uniform float u_VoxelSize;
uniform vec3 u_AabbMin;

// xy: direct diffuse, specular
// zw: indirect diffuse, specular
uniform vec4 u_LightingFactors;

LightContributions applyLight_pbr(vec3 aView, vec3 aDiffuseLightDir, vec3 aSpecularLightDir, vec3 aShadingNormal,
                                  PbrParameters aParams, LightColors aColors)
{
    LightContributions result;
    vec3 F;

    // IMPORTANT: All albedos are returned already multiplied by Pi
    // This already satisfy the Pi factor in the reflectance equation.

    // Specular and Fresnel
    {
        vec3 aLightDir = aSpecularLightDir;

        // Note: Lacking clear guidance, the Fresnel term is computed with the specular light dir
        //   The reasoning being that for practical purposes, this is the specular direction
        //   (and we want the energy trade-off with diffuse for the computed specular term)

        // TODO #glitch: The area light representative point made visible black dot artifacts on the
        //   sphere horizon when it is aligned to the light ("eclipse").
        //   I suppose one problem is the degenerate h vector when aView = -aLightDir (more probable with area)
        //   but there seem to be other underlying issue(s).
        vec3 h = normalize(aView + aLightDir);
        float hDotL = dotPlus(h, aLightDir);

        // Fresnel term `F` describe how the wave-length dependent reflectance (proportion of reflected light)
        // For microfacet BRDFs, we use dot(h, l), not dot(n, l), see: rtr 4th eq (9.63)
        F = schlickFresnelReflectance(hDotL, aParams.f0, aParams.f90);

        float nDotH = dotPlus(aShadingNormal, h);
        // Seems to fix some erroneous black pixels at horizon (but not all)
        float nDotL = max(0.001, dotPlus(aShadingNormal, aLightDir));
        float nDotV = dotPlus(aShadingNormal, aView);

        #if !defined(BLINNPHONG_BRDF)
            result.specular = specularBrdf_GGX(F, nDotH, nDotL, nDotV, aParams.alpha)
                              * aColors.specular.rgb
                              * nDotL
                              ;
        #else
            float nDotL_raw = dot(aShadingNormal, aLightDir);
            float nDotV_raw = dot(aShadingNormal, aView);

            float alpha_b = alpha / 1.7; // the magic denominator was manually tweaked to mostly match
            result.specular = specularBrdf_BlinnPhong(F, nDotH, nDotL_raw, nDotV_raw, alpha_b)
                              * aColors.specular.rgb
                              * nDotL;
        #endif // GGX_BRDF / BLINNPHONG_BRDF
    }

    // Diffuse
    {
        // Note: From rtr 4th structure and references to specular in Karis, we assume that 
        //   diffuse term should still use the punctual light direction
        //   (also, it seems wrong otherwise).
        vec3 aLightDir = aDiffuseLightDir;

        float nDotL = dotPlus(aShadingNormal, aLightDir);
        result.diffuse  = diffuseBrdf_weightedLambertian(F, aParams.diffuseColor)
                          * aColors.diffuse.rgb
                          * nDotL;
    }

    return result;
}


LightContributions applyIndirectLight_pbr(vec3 aPosition_aabb,
                                          vec3 aView_world,
                                          vec3 aShadingNormal_world,
                                          PbrParameters aParams,
                                          float aRoughness,
                                          out float aAmbientOcclusionFactor)
{
    LightContributions result;
    vec3 F;

    // Specular and Fresnel
    {
        vec3 reflectionDir = reflect(-aView_world, aShadingNormal_world);
        vec3 lightDir = reflectionDir;

        // Note: by construction here h == n, so we could save a few instructions
        vec3 h = normalize(aView_world + lightDir);
        float hDotL = dotPlus(h, lightDir);

        // Fresnel term `F` describe how the wave-length dependent reflectance (proportion of reflected light)
        // For microfacet BRDFs, we use dot(h, l), not dot(n, l), see: rtr 4th eq (9.63)
        F = schlickFresnelReflectance(hDotL, aParams.f0, aParams.f90);

        float nDotL = max(0.001, dotPlus(aShadingNormal_world, lightDir));

		vec3 specularRadiance = 
            accumulateSpecularIndirect(aPosition_aabb,
                                       aShadingNormal_world,
                                       -aView_world,
                                       aRoughness,
                                       u_VoxelSize).xyz;
        if (u_SplitSumIndirectSpecular)
        {
            // Note: Unlike what is presented in Finn, Johannes. “Evaluation of Performance and Image Quality for Voxel Cone Tracing,” n.d.
            // we do not multiply IntegragetBrdf.x with F term (as it is supposed to be pre-integrated in the LUT)
            result.specular =
                specularBrdfLut(aParams.f0,
                                // By construction, lightDir and viewDir are symetric around the normal
                                // so NoL == NoV
                                nDotL,
                                aRoughness,
					            u_IntegratedEnvironmentBrdf)
                * specularRadiance
                ;
        }
        else
        {
			result.specular = specularRadiance
							  * F
							  * nDotL
							  ;
        }

        //result.specular = vec3(F);
    }

    //// Diffuse
    {
        vec4 diffuse = accumulateDiffuseIndirect(aPosition_aabb,
                                                 aShadingNormal_world,
                                                 u_TanHalfAperture,
                                                 u_VoxelSize);
        aAmbientOcclusionFactor = 1 - diffuse.a;
        result.diffuse = diffuse.rgb
                         * (1 - F)
                         * aParams.diffuseColor
                         ;
    }

    return result;
}


void main(void)
{
    const uint gNoTextureChannel = uint(-1);

    MaterialGeneric material = ub_MaterialGeneric[u_MaterialIdx];

    vec4 albedo = ex_Color;
    if(u_DiffuseUvChannel != gNoTextureChannel)
    {
        albedo *= texture(u_DiffuseTexture, ex_Uv01);
    }

    //
    // alpha testing for cutout
    //
    if (albedo.a < 0.5)
    {
        discard;
    }


    //
    // Normals
    //
    vec3 shadingNormal_view;
    if(u_NormalUvChannel != gNoTextureChannel && u_ApplyNormalMap)
    {
        #define BC5_RGTC;
        #if defined(BC5_RGTC)
            // Fetch from Red-Green channels, and remap from [0, 1]^2 to [-1, 1]^2.
            vec2 normalXY = 
                texture(u_NormalTexture, ex_Uv01).xy
                * 2.0 - vec2(1.0);

            // Derives the third component from the two others, assuming the source normal map data was normalized
            vec3 normal_tbn = vec3(normalXY, sqrt(1.0 - dot(normalXY, normalXY)));
        #else
            // Fetch from normal map, and remap from [0, 1]^3 to [-1, 1]^3.
            vec3 normal_tbn = 
                texture(u_NormalTexture, ex_Uv01).xyz
                * 2 - vec3(1);
        #endif //BC5_RGTC

        // MikkT see: http://www.mikktspace.com/

        vec3 normal_view = ex_Normal_view;
        vec3 tangent_view = ex_Tangent_view;
        //#define COMPUTE_BITANGENT
        #ifdef COMPUTE_BITANGENT
            // TODO handle handedness, which should be -1 or 1
            //float handedness
            //vec3 bitangent_cam = cross(normal_cam, tangent_cam) * handedness;
        #else
            vec3 bitangent_view = ex_Bitangent_view;
        #endif

        #define NORMALIZE_TBN
        #ifdef NORMALIZE_TBN
            // Despite MikkT guideline, if the tangent and normal were not normalized
            // the result was be abherent with sample gltf assets (e.g. avocado, sponza) 
            normal_view    = normalize(normal_view);
            tangent_view   = normalize(tangent_view);
            bitangent_view = normalize(bitangent_view);
        #endif

        vec3 bumpNormal_cam = normalize(
              normal_tbn.x * tangent_view
            + normal_tbn.y * bitangent_view
            + normal_tbn.z * normal_view
        );

        shadingNormal_view = bumpNormal_cam;
    }
    else
    {
        shadingNormal_view = normalize(ex_Normal_view);
    }

    vec3 viewDir_view = normalize(-ex_Position_view);

    // Accumulators for the lights contributions
    vec3 diffuseAccum = vec3(0.);
    vec3 specularAccum = vec3(0.);


    //
    // Metallic-Roughness parameterization 
    //

    // TODO: implement mrao texture
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;

    if(u_MraoUvChannel != gNoTextureChannel)
    {
        vec4 mrao = texture(u_MraoTexture, ex_Uv01);
        // glTF sponza channel order
        metallic *= mrao.b;
        roughness *= mrao.g;
    }

    // Handle alpha
    // We assume the roughness, not alpha, is provided even in 3rd party assets.
    float alpha = alphaFromRoughness(roughness);

    // Note: Too smooth a surface (i.e too low an alpha)
    // makes it that there is not even a specular highlight showing with most models
    // (at least GGX & Blinn-Phong)
    // So, uncomment to fix it (e.g. the display on the glTF water bottle)
    alpha = max(0.005, alpha);

    // We blendg the parameters before computing the lighting model.
    // This is not physically correct (parameters do not have linear relationship to output)
    // but this is fast and results are still convincing. 
    PbrParameters pbrParameters;
    pbrParameters.diffuseColor = mix(albedo.rgb, vec3(0.), metallic);
    pbrParameters.f0 = mix(gF0_dielec, albedo.rgb, metallic);
    pbrParameters.f90 = gF90;
    pbrParameters.alpha = alpha;


    // 
    // PBR shading model (light simulation)
    //

    // Potentially used by direct lighting for cone traced shadows
    vec3 position_aabb = ex_Position_world - u_AabbMin;
    // TODO: Address this expensive calculation. Should everything happen in world space?
    vec3 geometricNormal_world = mat3(ub_cameraToWorld) * normalize(ex_Normal_view);
    vec3 shadingNormal_world = mat3(ub_cameraToWorld) * shadingNormal_view;

    // Directional lights
    for(uint directionalIdx = 0; directionalIdx != ub_DirectionalCount.x; ++directionalIdx)
    {
        DirectionalLight directional = ub_DirectionalLights[directionalIdx];
        vec3 lightDir_view = -ub_Directions_view[directionalIdx].xyz;
        
        LightContributions lighting = 
            applyLight_pbr(
                viewDir_view, lightDir_view, lightDir_view, shadingNormal_view,
                pbrParameters, directional.colors);

		switch(u_ShadowMethod)
		{
		case CLIENT_SHADOW_SHADOWMAP:
			#if defined(SHADOW_MAPPING)
				if(directionalIdx < MAX_SHADOW_LIGHTS)
				{
					applyShadowToDirectionalLighting(lighting, directionalIdx);
				}
			#endif // SHADOW_MAPPING
			break;
		case CLIENT_SHADOW_CONETRACING:
			float shadowFactor = 
				traceShadow(position_aabb, geometricNormal_world, 
							-directional.direction.xyz, u_TanHalfShadow,
							u_VoxelSize);
			scale(lighting, shadowFactor);
			break;
		}

        diffuseAccum += lighting.diffuse;
        specularAccum += lighting.specular;
    }


    // Point lights
    for(uint pointIdx = 0; pointIdx != ub_PointCount.x; ++pointIdx)
    {
        PointLight point = ub_PointLights[pointIdx];

        // see rtr 4th p110 (5.10)
        vec3 lightRay_view = ub_Points_view[pointIdx].xyz - ex_Position_view;
        float radius = length(lightRay_view);
        vec3 lightDir_view = lightRay_view / radius;

        vec3 specularLightDir_view = normalize(
            representativePoint_sphere(ex_Position_view,
                                       ub_Points_view[pointIdx].xyz,
                                       reflect(-viewDir_view, shadingNormal_view),
                                       point.radius.x));

        LightContributions lighting = 
            applyLight_pbr(
                viewDir_view, lightDir_view, specularLightDir_view, shadingNormal_view,
                pbrParameters, point.colors);

        if(pointIdx < MAX_SHADOW_LIGHTS)
        {
			applyShadowToPointLighting(lighting, pointIdx, ex_Position_world);
		}

        float falloff = attenuatePoint(point, radius);
        diffuseAccum  += lighting.diffuse  * falloff;
        specularAccum += lighting.specular * falloff;
    }

    diffuseAccum *= u_LightingFactors.x;
    specularAccum *= u_LightingFactors.y;

    //
    // Indirect lighting (VXGI)
    //
    vec3 view_world = normalize(getCameraPosition_world() -  ex_Position_world);

    float voxelAoFactor = 1;

    LightContributions indirect = 
        applyIndirectLight_pbr(position_aabb,
                               view_world,
                               shadingNormal_world,
                               pbrParameters,
                               roughness,
                               voxelAoFactor);

    diffuseAccum += 
        indirect.diffuse
        * voxelAoFactor
        * u_LightingFactors.z
        ;
    specularAccum += 
        indirect.specular
        * u_LightingFactors.w
        ;

    //out_Color = vec4(indirect.specular, 1);
    //return;

    // Sum contributions
    // Note: the ambient term is a quick hack, to be removed when IBL is in place
    // We multiply it by the diffuse color, so metals do not have ambient terms, and dielectrics have their tint.
    vec3 ambient =  ub_AmbientColor.rgb * material.ambientColor.rgb 
                    * pbrParameters.diffuseColor
                    * voxelAoFactor;
    vec3 diffuse  = diffuseAccum        * material.diffuseColor.rgb;
    // TODO: what is this specularColor factor?
    vec3 specular = specularAccum       * material.specularColor.rgb;

    //
    // Ambient Occlusion
    //

    float aoFactor;
    if(u_ApplyAo)
    {
        vec2 frag_screenuv = gl_FragCoord.xy / u_FramebufferSize;
        aoFactor = texture(u_AmbientOcclusion, frag_screenuv).r;
        ambient *= aoFactor;
    }

    vec3 fragmentColor = diffuse + ambient + specular;

    // DEBUG SECTION
    //fragmentColor = specular;
    //fragmentColor = highlightAberrations(fragmentColor);


    //
    // IBL
    //
    #if defined(ENVIRONMENT_MAPPING)
    if(u_ApplyEnvironment)
    {
        // The directions that will be used to sample into the cubemap need to be in 
        // world-space, where the cubemaps are defined.
        // Note: No need to normalize as long as it is only used to sample a cubemap.
        vec3 reflected_world = mat3(ub_cameraToWorld) * reflect(-viewDir_view, shadingNormal_view);
        vec3 shadingNormal_world = mat3(ub_cameraToWorld) * shadingNormal_view;

        vec3 specularIbl = 
            approximateSpecularIbl(pbrParameters.f0,
                                   reflected_world,
                                   shadingNormal_world,
                                   // Note should be reused from previous computation
                                   // (but is currently calculated inside a function)
                                   dotPlus(shadingNormal_view, viewDir_view),
                                   roughness,
                                   u_FilteredRadianceEnvironmentTexture,
                                   u_IntegratedEnvironmentBrdf);

        vec3 irradianceIbl = texture(u_FilteredIrradianceEnvironmentTexture, 
                                     worldToCubemap(shadingNormal_world)).rgb;

        if(u_ApplyAo)
        {
            // See rtr 4th eq. (11.25) p464
            // TODO: There is a 1/Pi factor in the book equation. Is it already part of the integrated irradiance
            irradianceIbl *= aoFactor;
        }


        // Note: The Fresnel term:
        //   * is part of the precomputed BRDF of split-sum approximation for specular
        //   * might be part of the irradiance texture for diffuse (see: prefilterEnvMapDiffuse_LambertianFresnel())
        // Note: mftpbr does use Fresnel terms in its diffuses brdf (Fr_DisneyDiffuse),
        //   which is in a term separate from the actual image lighting pre-integration.

        fragmentColor += specularIbl * u_SpecularIblFactor;
        // For Lambertian surfaces, outgoing radiance is proportional to irradiance.
        // See rtr 4th eq. (10.2) p379
        fragmentColor += irradianceIbl
                          // Diffuse color is the subsurface albedo
                         * pbrParameters.diffuseColor.rgb
                         * u_DiffuseIblFactor
                         ;
    }
    #endif //ENVIRONMENT_MAPPING

    //
    // Output
    //
    switch(u_ToneMapping)
    {
    case CLIENT_TONEMAPPING_REINHARD:
        fragmentColor = tonemapReinhard(fragmentColor);
        break;
    case CLIENT_TONEMAPPING_ACES:
        fragmentColor = tonemapAces(fragmentColor);
    case CLIENT_TONEMAPPING_ACESAPPROX:
        fragmentColor = tonemapAces_approx(fragmentColor);
        break;
    // Default is none
    }
    out_Color = correctGamma(vec4(fragmentColor, albedo.a));
}
