#version 430 core

in vec2 pos;
out vec2 uv;


void main()
{
	uv = 0.5 * pos + 0.5;
	gl_Position = vec4(pos, 0.0, 1.0);
}

