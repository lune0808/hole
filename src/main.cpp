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
uniform layout(binding=0,r32f) writeonly restrict image2D screen;
void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, vec4(1.0, 0.0, 0.0, 0.0));
}
)",

R"(
#version 430 core
in vec2 pos;
out vec2 uv;
void main()
{
	uv = 0.5 * pos + vec2(0.5, 0.5);
	gl_Position = vec4(pos, 0.0, 1.0);
}
)",

R"(
#version 430 core
uniform layout(binding=0) sampler2D screen;
in vec2 uv;
out vec4 f_color;
void main()
{
	f_color = texture(screen, uv);
}
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

static const float quad[] = {
	-1.0f, -1.0f,
	+1.0f, -1.0f,
	-1.0f, +1.0f,

	-1.0f, +1.0f,
	+1.0f, -1.0f,
	+1.0f, +1.0f,
};

GLuint describe_va()
{
	GLuint va;
	glGenVertexArrays(1, &va);
	glBindVertexArray(va);

	GLuint vb;
	glGenBuffers(1, &vb);
	glBindBuffer(GL_ARRAY_BUFFER, vb);
	glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(0);

	return va;
}

int main()
{
	const int width = 800;
	const int height = 600;
	window win(width, height);
	const auto compute_shdr = build_shader(source[shader::compute]);
	const auto graphics_shdr = build_shader(source[shader::vertex], source[shader::fragment]);
	camera_t camera{
		0.78f, float(width), float(height),
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f,-1.0f },
	};

	GLuint texture;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, width, height);
	glBindImageTexture(0 /* cs binding */, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

	const auto va = describe_va();

	while (win) {
		glUseProgram(compute_shdr);
		glDispatchCompute(width, height, 1);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

		glUseProgram(graphics_shdr);
		glBindVertexArray(va);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		win.draw();
	}
}

