#include <cstdio>
#include <thread>
#include <cstddef>
#include <cassert>
#include <numbers>
#include <glm/glm.hpp>
#include "window.hpp"
#include "shader.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

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

	static constexpr size_t n_frames = 48;

	GLuint sim;
	glGenTextures(1, &sim);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, sim);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA32F, width, height, n_frames);

	GLuint skybox;
	glGenTextures(1, &skybox);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
	static const char *const skybox_paths[6] = {
		"res/sky_right.png",
		"res/sky_left.png",
		"res/sky_top.png",
		"res/sky_bottom.png",
		"res/sky_front.png",
		"res/sky_back.png",
	};
	for (size_t i = 0; i < std::size(skybox_paths); ++i) {
		int face_width, face_height, face_channels;
		auto face = stbi_load(skybox_paths[i], &face_width, &face_height, &face_channels, 0);
		assert(face);
		static const GLenum formats[] = {
			GL_RED, GL_RG, GL_RGB, GL_RGBA,
		};
		assert(0 <= face_channels && face_channels <= std::size(formats));
		const auto format = formats[face_channels-1];
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0, format, face_width, face_height, 0, format, GL_UNSIGNED_BYTE, face);
		stbi_image_free(face);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	struct
	{
		glm::vec3 cam_right;
		float inv_screen_width;
		glm::vec3 cam_up;
		float focal_length;
		glm::vec3 cam_pos;
		float sch_radius;
		glm::vec3 sphere_pos;
		float sphere_r;
		glm::vec4 q_orientation;
		GLuint iterations;
	} data;

	const glm::vec3 x{1.0f, 0.0f, 0.0f};
	const glm::vec3 y{0.0f, 1.0f, 0.0f};
	const glm::vec3 z{0.0f, 0.0f, 1.0f};
	data.cam_right = x;
	data.cam_up = y;
	data.sphere_r = 0.2f;
	data.sphere_pos = glm::vec3{data.sphere_r, 0.0f, -1.0f};
	static constexpr float fov = std::numbers::pi_v<float> / 3.0f;
	data.inv_screen_width = 1.0f / float(width);
	data.focal_length = 0.5f / std::tan(fov * 0.5f);
	data.sch_radius = 0.2f;
	data.iterations = 96;
	data.q_orientation = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};

	GLuint ssb;
	glGenBuffers(1, &ssb);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof data, &data, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1 /* binding */, ssb);

	glUseProgram(compute_shdr);
	glUniform1i(2 /* skybox */, 0 /* GL_TEXTURE0 */);
	glUseProgram(graphics_shdr);
	glUniform1i(0 /* graphics binding for screen */, 1 /* GL_TEXTURE1 */);

	const auto va = describe_va();

	const glm::vec3 start_pos = +1.0f*z + 0.6f*y;
	const glm::vec3 end_pos = -0x1.0p-10f*x -0.95f*z - 0.015f*y;
	for (size_t i_frame = 0; win && i_frame < n_frames; ++i_frame) {
		glBindImageTexture(0 /* cs binding */, sim, 0, GL_FALSE, i_frame, GL_WRITE_ONLY, GL_RGBA32F);
		glUseProgram(compute_shdr);
		float progress = float(i_frame) / float(n_frames-1);
		float angle = progress * std::numbers::pi_v<float> / 4.0f;
		data.q_orientation = glm::vec4{std::sin(angle * 0.5f) * y, std::cos(angle * 0.5f)};
		data.cam_pos = glm::mix(start_pos, end_pos, progress);
		// data.sch_radius = glm::mix(0.0f, 0.25f, progress*progress);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof data, &data);
		glDispatchCompute(width, height, 1);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

		glUseProgram(graphics_shdr);
		glUniform1f(1 /* location for frame */, float(i_frame));
		glBindVertexArray(va);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		win.draw();

		std::printf("\rframe #%zu", i_frame);
		std::fflush(stdout);
	}
	std::printf("\r                 \r");

	size_t cur_frame = n_frames-1;
	bool up = false;
	while (win) {
		if (up) {
			cur_frame++;
		} else {
			cur_frame--;
		}
		if (cur_frame == 0 || cur_frame == n_frames-1) {
			up ^= true;
		}
		glUseProgram(graphics_shdr);
		glUniform1f(1 /* location for frame */, float(cur_frame));
		glBindVertexArray(va);
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(50ms);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		win.draw();
	}
}

