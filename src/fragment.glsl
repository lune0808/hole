#version 430 core

uniform layout(binding=0) sampler2D screen;
in vec2 uv;
out vec4 f_color;


void main()
{
	f_color = texture(screen, uv);
}

