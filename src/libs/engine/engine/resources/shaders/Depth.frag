#version 460


in vec3 ex_Position_view;
in vec3 ex_Normal_view;

layout(location = 0) out vec3 out_Position;
layout(location = 1) out vec3 out_Normal;

void main(void)
{
    // TODO
    //#if defined(LINEAR_DEPTH)
    //    gl_FragDepth = ex_Position_view.z / ex_Position_view.w;
	//#endif

    // Otherwsie, no need to write to gl_FragDepth, the depth buffer is updated automatically.

    #if defined(OUTPUT_FRAGMENT_VIEW_POSITION)
        out_Position = ex_Position_view;
        out_Normal = ex_Normal_view;
    #endif
}