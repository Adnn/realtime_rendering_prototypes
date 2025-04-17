struct LineSegment
{
    // We never use vec3 in buffer-backed interface blocks due to alignment complications
    // We could use the last float for a per-point width
    vec4 pointA;
    vec4 pointB;
    vec4 color;
    float width;
};


layout(std140, binding = 8) readonly buffer LinesSsbo
{
    LineSegment ub_Segments[];
};
