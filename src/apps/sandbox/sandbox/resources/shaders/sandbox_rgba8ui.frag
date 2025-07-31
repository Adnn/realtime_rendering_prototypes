#version 460

uniform ivec2 u_FramebufferSize;

uniform sampler2D u_Texture1;
uniform usampler2D u_Texture2;

out vec4 out_Color;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_FramebufferSize;
	#define RGBA8UI
    #if defined(RGBA8UI)
		// Need to convert to vec4 to get floating point division instead of integer
		out_Color = vec4(texture(u_Texture2, uv)) / 255;
	#else
		out_Color = texture(u_Texture1, uv);
	#endif
}