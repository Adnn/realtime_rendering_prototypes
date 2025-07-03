#version 460


#include "ViewProjectionBlock.glsl"


layout(location=0) in vec3 ve_Position;
layout(location=1) in vec3 ve_Normal;
layout(location=2) in vec4 ve_Color;
layout(location=3) in vec3 ve_Tangent;
layout(location=4) in vec3 ve_Bitangent;
layout(location=5) in vec2 ve_Uv01;

#if defined(ENTITIES)
    // Note: currently replaced by gl_InstanceID because we have 1:1 mapping between instances and entities
    // The instance attribute associating the OpenGL instance
    // (in the sense of instanced rendering) to its corresponding Entity.
    //layout(location=1) in uint in_EntityIdx; // Note: cannot be part of the include, which might be used in other shader stages
    #define ENTITY_IDX_ATTRIBUTE (gl_InstanceID + gl_BaseInstance)
    #include "EntitiesBlock.glsl"
#else
    // Hopefully optimized away
    float getModelTransform()
    { return 1; }
#endif // ENTITIES

#if defined(SHADOW_MAPPING)
	#include "LightViewProjectionBlock.glsl"
	out vec3[MAX_SHADOW_MAPS] ex_Position_lightTex;
#endif //SHADOW_MAPPING

// Output interpolated for fragment shader
// Note: we duplicate outputs that are of interest to the geometry shader.
//   This ways the geometry stage is optional,:
//     - vertex still defines ex_ as output, so it can be directly followed by fragment expecting ex_ as input
//     - geometry can use gi_ as input, and define ex_ as output (no clashing in the names)  
//     - we wishfully expect the linker to prune the unused outputs
out vec4 ex_Color;
out vec4 gi_Color;
out vec3 ex_Position_world;
out vec3 ex_Normal_world;
out vec3 ex_Position_view;
out vec3 ex_Normal_view;
out vec3 gi_Normal_view;
out vec3 ex_Tangent_view;
out vec3 ex_Bitangent_view;
out vec2 ex_Uv01;
out vec2 gi_Uv01;


void main(void)
{
    ex_Color = vec4(1)
        #if defined(ENTITIES)
            * getEntity().colorFactor
        #endif
        #if defined(VERTEX_COLOR)
            * ve_Color
        #endif
        ;
    gi_Color = ex_Color;

    // TODO: handle non-uniform scaling with dedicated normal transform
    vec4 normal_world = getModelTransform() * vec4(ve_Normal, 0);
    ex_Normal_world = normalize(normal_world.xyz);
    ex_Normal_view = vec3(ub_worldToCamera * normal_world);
    gi_Normal_view = ex_Normal_view;

    // Tangents are transformed like positions, by the localToCamera matrix (unlike normals)
    // see: https://www.pbr-book.org/3ed-2018/Geometry_and_Transformations/Applying_Transformations
    mat4 localToCamera = ub_worldToCamera * getModelTransform();
    ex_Tangent_view   = normalize(mat3(localToCamera) * ve_Tangent);
    ex_Bitangent_view = normalize(mat3(localToCamera) * ve_Bitangent);

    vec4 position_world = getModelTransform() * vec4(ve_Position, 1.);
    ex_Position_world = position_world.xyz;
    vec4 position_view = ub_worldToCamera * position_world;
    ex_Position_view = position_view.xyz;

    #if defined(SHADOW_MAPPING)
        for(uint lightIdx = 0; lightIdx != ub_LightViewProjectionCount; ++lightIdx)
        {
            vec4 position_lightClip = ub_LightViewProjections[lightIdx] * position_world;
            // The position is in homogeneous clip space (from the light POV).
            // We apply the perspective divide to get to NDC, and remap from [-1, 1]^3 to [0, 1]^3.
            // Note: even the depth (z) is remapped to [0, 1], because the viewport transformation
            // did it for the depth values stored in the shadow map.
            ex_Position_lightTex[lightIdx] = 
                (position_lightClip.xyz / position_lightClip.w + 1.) / 2.;
        }
    #endif //SHADOW_MAPPING

    ex_Uv01 = ve_Uv01;
    gi_Uv01 = ex_Uv01;

    gl_Position = ub_projection * position_view;
}
