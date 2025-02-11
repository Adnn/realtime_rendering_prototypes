#pragma once


#include <renderer/GL_Loader.h>


namespace ad {

const GLchar* gVertexShader = R"#(
    #version 460

    layout(location=0) in vec3 in_Position;
    layout(location=1) in vec3 in_Color;

    layout(std140, binding=0) uniform ViewProjectionBlock
    {
        mat4 worldToCamera;
        mat4 cameraToWorld;
        mat4 projection;
        mat4 viewingProjection;
    };

    out vec3 ex_Color;

    void main(void)
    {
        ex_Color = in_Color;
        gl_Position = viewingProjection * vec4(in_Position, 1.0);
    }
)#";

const GLchar* gFragmentShader = R"#(
    #version 460

    in vec3 ex_Color;

    out vec4 out_Color;

    void main(void)
    {
        out_Color = vec4(ex_Color, 1.0);
    }
)#";


} // namespace ad
