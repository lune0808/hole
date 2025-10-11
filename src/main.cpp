#include <cstdio>
#include <cassert>
#include <glm/glm.hpp>
#include "window.hpp"
#include "shader.hpp"

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
	auto compute_src = load_file("src/compute.glsl");
	const auto compute_shdr = build_shader(compute_src);
	delete[] compute_src;
	auto vertex_src = load_file("src/vertex.glsl");
	auto fragment_src = load_file("src/fragment.glsl");
	const auto graphics_shdr = build_shader(vertex_src, fragment_src);
	delete[] vertex_src;
	delete[] fragment_src;
	camera_t camera{
		1.05f, float(width), float(height),
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f,-1.0f },
	};

	GLuint texture;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width, height);
	glBindImageTexture(0 /* cs binding */, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	struct
	{
		glm::vec3 cam_right;
		float inv_screen_width;
		glm::vec3 cam_up;
		float focal_length;
		glm::vec3 cam_pos;
		float __padding;
	} data;
	const glm::vec3 y{0.0f, 1.0f, 0.0f};
	data.cam_right = glm::normalize(glm::cross(camera.direction, y));
	data.cam_up = glm::cross(data.cam_right, camera.direction);
	data.cam_pos = camera.position;
	data.inv_screen_width = 1.0f / camera.width;
	data.focal_length = 0.5f / std::tan(camera.fov * 0.5f);

	GLuint ssb;
	glGenBuffers(1, &ssb);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof data, &data, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1 /* binding */, ssb);

	glUseProgram(graphics_shdr);
	glUniform1i(0 /* graphics binding for screen */, 0 /* GL_TEXTURE0 */);

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

