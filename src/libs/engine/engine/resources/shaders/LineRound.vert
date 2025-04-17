#version 460


#include "Constants.glsl"
#include "ViewProjectionBlock.glsl"


layout(location = 0) in vec3 v_Position;

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


// Note: the idea to use a special pattern of positions to draw the line and the round endpoints
// was inspired by: https://wwwtyro.net/2019/11/18/instanced-lines.html
void main() 
{
    ex_Color = vec4(1);

    LineSegment segment = ub_Segments[gl_InstanceID];
    float width = segment.width;
    vec3 point = v_Position;

    // Use a billboarding technique, and does not requires any extra transformation
    // while giving equivalent results to the screen-space computation
    vec4 A_world = vec4(segment.pointA.xyz, 1);
    vec4 B_world = vec4(segment.pointB.xyz, 1);
    vec4 selectedPoint_world = (point.z == 1 ? B_world : A_world);

    vec3 xBasis_world = normalize((B_world - A_world).xyz);
    // Extract the camera position (last column of the cameraToWorld matrix)
    vec3 camera_world = ub_cameraToWorld[3].xyz;
    vec3 quadNormal_world = camera_world - selectedPoint_world.xyz;

    vec3 yBasis_world = normalize(cross(quadNormal_world, xBasis_world));
    xBasis_world = normalize(cross(yBasis_world, quadNormal_world));

    vec3 offset_world = width * (point.x * xBasis_world + point.y * yBasis_world);
    vec4 vertex = selectedPoint_world + vec4(offset_world, 0);

    gl_Position = ub_viewingProjection * vertex;
}
