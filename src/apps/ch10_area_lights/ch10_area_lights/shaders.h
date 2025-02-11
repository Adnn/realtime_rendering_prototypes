#pragma once


#include <renderer/GL_Loader.h>


namespace ad {

const GLchar* gVertexShader = R"#(
    #version 400

    layout(location=0) in vec3 in_Position;
    layout(location=1) in vec3 in_Color;

    out vec3 ex_Color;

    void main(void)
    {
        mat4 modelTransform = mat4(1.0);
        modelTransform[0][0] = 0.5;
        modelTransform[1][1] = 0.5;
        modelTransform[2][2] = 0.5;
        ex_Color = in_Color;
        gl_Position = modelTransform * vec4(in_Position, 1.0);
    }
)#";

const GLchar* gFragmentShader = R"#(
    #version 400

    in vec3 ex_Color;

    out vec4 out_Color;

    void main(void)
    {
        out_Color = vec4(ex_Color, 1.0);
    }
)#";


} // namespace ad
