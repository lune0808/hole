#version 430 core

uniform layout(binding=0) sampler2DArray screen;
uniform layout(location=1) float frame;
in vec2 uv;
out vec4 f_color;


void main()
{
	f_color = texture(screen, vec3(uv, frame));
}

