#version 430 core

uniform layout(binding=0) sampler2DArray screen0;
uniform layout(binding=1) sampler2DArray screen1;
uniform layout(location=2) float frame;
uniform layout(location=3) float select;
in vec2 uv;
out vec4 f_color;


void main()
{
	vec3 coord = vec3(uv, frame);
	vec4 color0 = texture(screen0, coord);
	vec4 color1 = texture(screen1, coord);
	f_color = mix(color0, color1, select);
}

