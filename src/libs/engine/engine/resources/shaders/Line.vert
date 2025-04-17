#version 460


#include "ViewProjectionBlock.glsl"


out vec2 ex_Uv;
out vec4 ex_Color;

uniform ivec2 u_FramebufferSize;

struct LineSegment
{
    // We never use vec3 in buffer-backed interface blocks due to alignment complications
    // We could use the last float for a per-point width
    vec4 pointA;
    vec4 pointB;
    float width;
};


layout(std140, binding = 8) readonly buffer LinesSsbo
{
    LineSegment ub_Segments[];
};

const vec2 pos_data[4] = vec2[] (
    vec2( 1.0, -0.5),
    vec2( 1.0,  0.5),
    vec2( 0.0, -0.5),
    vec2( 0.0,  0.5)
);

const vec2 tex_data[4] = vec2[] (
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 0.0),
    vec2(0.0, 1.0)
);


void main() 
{
    ex_Uv = tex_data[gl_VertexID];
    ex_Color = vec4(1);

    LineSegment segment = ub_Segments[gl_InstanceID];
    float width = segment.width;
    vec2 point = pos_data[gl_VertexID];

//#define COMPUTE_VIEW_SPACE
//#define COMPUTE_SCREEN_SPACE
#if defined(COMPUTE_VIEW_SPACE)
	// This approach computes the line side vector in view space
	// The problem is it make the singularity (when the segment is along Z)
	// obvious with a perspective projection (good result in orthographic case)

    vec4 A_view = ub_worldToCamera * vec4(segment.pointA.xyz, 1);
    vec4 B_view = ub_worldToCamera * vec4(segment.pointB.xyz, 1);

    // Ray is the main direction along the line segment, side is the perpendicular direction (width)
    vec4 ray_view = B_view - A_view;
    vec4 side_view = vec4(normalize(vec2(-ray_view.y, ray_view.x)), 0, 0);

	//#define WIDTH_IN_VIEW
	#if defined(WIDTH_IN_VIEW)
		vec4 sideScaled_view = side_view * width;
	#else
		// Note: we cannot directly comput side in world coordinates, 
		// since it would likely not result in a quad facing the camera
		vec4 side_world = ub_cameraToWorld * side_view;
		vec4 sideScaled_view = ub_worldToCamera * (side_world * width);
	#endif

    gl_Position = 
        ub_projection
        * (A_view + ray_view * point.x + sideScaled_view * point.y);

#elseif defined(COMPUTE_SCREEN_SPACE)
    // This approach computes the side vector in screen (window) space
    // wich is the space where the segment is 2D 
    // (the singularity is when the line is viewed head-on)
    vec4 A_clip = ub_viewingProjection * vec4(segment.pointA.xyz, 1);
    vec4 B_clip = ub_viewingProjection * vec4(segment.pointB.xyz, 1);

    vec2 A_screen = u_FramebufferSize * (0.5 * A_clip.xy / A_clip.w + 0.5);
    vec2 B_screen = u_FramebufferSize * (0.5 * B_clip.xy / B_clip.w + 0.5);

    vec2 ray_screen = B_screen - A_screen;
    // Note: we will normalize the side vector in world space, before scaling by width
    vec2 side_screen = vec2(-ray_screen.y, ray_screen.x);

    vec4 clp = mix(A_clip, B_clip, point.x);
    // This would be to transform a position
    //vec4 side_clip = vec4(clp.w * (2 * side_screen / u_FramebufferSize - 1),
    //                      clp.z, clp.w);
    // Since we are transforming a vector, we do not substract the origin (the inverse of the +0.5)
    // Note: we actually do not even need to scale .xy by w, since we will normalize the result
    vec4 side_clip = vec4(/*clp.w * */(2 * side_screen / u_FramebufferSize),
                          0, 0);
    // Scale the side vector by the segment width in **world space**
    vec4 side_world = ub_cameraToWorld * inverse(ub_projection) * side_clip;
    vec4 sideScaled_clip = ub_viewingProjection * (normalize(side_world) * width);

    vec4 ray_clip = B_clip - A_clip;
    gl_Position = (A_clip + ray_clip * point.x + sideScaled_clip * point.y);

#else
    // This one use a billboarding technique, and does not requires any extra transformation
    // while giving equivalent results to the screen-space computation
    vec4 A_world = vec4(segment.pointA.xyz, 1);
    vec4 B_world = vec4(segment.pointB.xyz, 1);

    vec4 ray_world = B_world - A_world;
    // Extract the camera position (last column of the cameraToWorld matrix)
    vec3 camera_world = ub_cameraToWorld[3].xyz;
    vec3 quadNormal_world = camera_world - A_world.xyz;

    vec3 side_world = normalize(cross(quadNormal_world, ray_world.xyz));
	vec4 sideScaled_world = vec4(side_world * width, 0);

    gl_Position = 
        ub_viewingProjection
        * (A_world + ray_world * point.x + sideScaled_world * point.y);
#endif
}
