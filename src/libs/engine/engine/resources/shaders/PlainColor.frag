#version 460

in vec4 ex_Color;

out vec4 out_Color;

void main(void)
{
	#if defined(GAMMA_CORRECTION)
		out_Color = correctGamma(ex_Color);
	#else
		out_Color = ex_Color;
	#endif // GAMMA_CORRECTION
}
