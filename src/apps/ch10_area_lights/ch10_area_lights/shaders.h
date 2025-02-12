#pragma once


#include <renderer/GL_Loader.h>


namespace ad {

const GLchar* gVertexShader = R"#(
    #version 460

    layout(location=0) in vec3 in_Position;
    layout(location=1) in vec3 in_Color;

    out vec3 vPosition;
    out vec3 vColor;

    void main(void)
    {
        vColor = in_Color;
        vPosition = in_Position;
    }
)#";


const GLchar* gTessellationEvaluationShader = R"#(
    #version 460

    //layout(triangles, fractional_odd_spacing) in;
    layout(triangles, equal_spacing) in;

    // Input from tessellation control shader or vertex shader
    in vec3 vPosition[];
    in vec3 vColor[];

    layout(std140, binding=0) uniform ViewProjectionBlock
    {
        mat4 worldToCamera;
        mat4 cameraToWorld;
        mat4 projection;
        mat4 viewingProjection;
    };

    // Output interpolated for fragment shader
    out vec3 ex_Position;
    out vec3 ex_Color;

    void main() {
        // Barycentric coordinates of the tessellated vertex
        float u = gl_TessCoord.x;
        float v = gl_TessCoord.y;
        float w = gl_TessCoord.z;

        // Interpolate positions from the patch
        vec3 pos = vPosition[0] * u + vPosition[1] * v + vPosition[2] * w;

        // Project onto the unit sphere
        ex_Position = normalize(pos);

        // Interpolate color
        ex_Color = vColor[0] * u + vColor[1] * v + vColor[2] * w;

        // Transform the tessellated vertex position to clip space
        gl_Position = viewingProjection * vec4(ex_Position, 1.0);
    }
)#";


const GLchar* gFragmentShader = R"#(
    #version 460

    in vec3 ex_Position;
    in vec3 ex_Color;

    out vec4 out_Color;

    void main(void)
    {
        out_Color = vec4(ex_Color, 1.0);
    }
)#";


} // namespace ad
