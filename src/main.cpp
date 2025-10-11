#include <cstdio>
#include <cassert>
#include <glm/glm.hpp>
#include "window.hpp"
#include "shader.hpp"

enum shader {
	compute,
	vertex,
	fragment,
};

static const char *source[] = {
R"(
#version 430 core
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
uniform layout(binding=0,rgba8ui) writeonly restrict uimage2D screen;
void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, ivec4(255,255,255,255));
}
)",
R"(
)",
R"(
)",
};

struct camera_t
{
	float fov;
	float width;
	float height;
	glm::vec3 position;
	glm::vec3 direction;
};

int main()
{
	const int width = 800;
	const int height = 600;
	window win(width, height);
	const auto compute_shdr = build_shader(source[shader::compute]);
	camera_t camera{
		0.78f, float(width), float(height),
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f,-1.0f },
	};

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, width, height);
	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI);

	glUseProgram(compute_shdr);

	while (win) {
		glDispatchCompute(width, height, 1);
		win.draw();
	}
}

