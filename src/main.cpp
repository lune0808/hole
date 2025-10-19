#include <cstdio>
#include <cstdint>
#include <cstring>
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

static float smoothstep(float x)
{
	// flat tangents at 0 & 1, mapping [0,1] to [0,1]
	return 3.0f * x * x - 2.0f * x * x * x;
}

struct dump_t {
	struct {
		std::int32_t width;
		std::int32_t height;
		std::int32_t frame_count;
		std::int32_t ms_per_frame;
	} header;
	std::unique_ptr<std::uint32_t[]> data;
};

size_t npixels(dump_t const &d)
{
	return d.header.width * d.header.height * d.header.frame_count;
}

dump_t dump(int width, int height, int frames, std::chrono::milliseconds dt, GLuint gl_name)
{
	dump_t d;
	d.header.width = width;
	d.header.height = height;
	d.header.frame_count = frames;
	d.header.ms_per_frame = dt.count();
	size_t count = npixels(d);
	d.data = std::make_unique<std::uint32_t[]>(count);
	glGetTextureImage(gl_name, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, count * sizeof(d.data[0]), d.data.get());
	return d;
}

int to_file(dump_t const &d, const char *path)
{
	const size_t pix = npixels(d);
	std::FILE *file = std::fopen(path, "w");
	if (!file) {
		return 1;
	}
	std::fwrite(&d.header, sizeof(d.header), 1, file);
	std::fwrite(d.data.get(), sizeof(std::uint32_t), pix, file);
	std::fclose(file);
	return 0;
}

dump_t from_file(const char *path)
{
		dump_t d;
		std::FILE *file = std::fopen(path, "r");
		if (!file) {
			return d;
		}
		std::fread(&d.header, sizeof(d.header), 1, file);
		const size_t count = npixels(d);
		d.data = std::make_unique<std::uint32_t[]>(count);
		std::fread(d.data.get(), sizeof(std::uint32_t), count, file);
		std::fclose(file);
		return d;
}

void upload(dump_t const &d, GLuint gl_name)
{
	glTextureSubImage3D(gl_name, 0 /* mipmap */,
		0, 0, 0, // x,y,z offsets
		d.header.width, d.header.height, d.header.frame_count, // x,y,z dimensions
		GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, d.data.get());
}

struct draw_settings {
	GLuint shader;
	GLuint quad_va;
	GLuint frame_location;
	float frame_value;
};

void draw_quad(draw_settings set)
{
	glUseProgram(set.shader);
	glUniform1f(set.frame_location, set.frame_value);
	glBindVertexArray(set.quad_va);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

struct back_and_forth {
	size_t cur;
	size_t max;
	size_t step;

	back_and_forth(size_t max): cur{0}, max{max-1}, step(1) {}

	size_t advance()
	{
		cur += step;
		if (0 == cur || cur == max) {
			step = -step;
		}
		return cur;
	}
};

GLuint graphics_shader(const char *vpath, const char *fpath)
{
	auto vsrc = load_file(vpath);
	auto fsrc = load_file(fpath);
	auto shdr = build_shader(vsrc, fsrc);
	delete[] vsrc;
	delete[] fsrc;
	return shdr;
}

static constexpr glm::vec3 X{1.0f, 0.0f, 0.0f};
static constexpr glm::vec3 Y{0.0f, 1.0f, 0.0f};
static constexpr glm::vec3 Z{0.0f, 0.0f, 1.0f};

static constexpr float start_sch_r = 2.0f;
static constexpr float   end_sch_r = 2.0f;

static constexpr float start_angle = 0.0f;
static constexpr float   end_angle = std::numbers::pi_v<float> / 6.0f;

static constexpr glm::vec3   end_pos = start_sch_r * (-2.0f*X+2.0f*Z);
static constexpr glm::vec3 start_pos = end_pos + start_sch_r * (+15.0f*Z+4.0f*Y);

static constexpr int default_width = 800;
static constexpr int default_height = 600;
static constexpr size_t default_frames = 48;
static constexpr size_t default_frame_time = 5000 / default_frames;
static constexpr size_t default_iterations = 256;

int main(int argc, char **argv)
{
	enum { OUTPUT, INPUT } mode = OUTPUT;
	const char *sim_path = "/tmp/black_hole_sim_data.bin";
	if (argc == 3 && std::strcmp(argv[1], "-o") == 0) {
		sim_path = argv[2];
	} else if (argc == 2) {
		sim_path = argv[1];
		mode = INPUT;
	}

	dump_t sim_repr;
	if (mode == INPUT) {
		sim_repr = from_file(sim_path);
		assert(sim_repr.data);
	}

	const int width = (mode == OUTPUT)? default_width: sim_repr.header.width;
	const int height = (mode == OUTPUT)? default_height: sim_repr.header.height;
	window win(width, height); // context creation, etc
	const auto graphics_shdr = graphics_shader("src/vertex.glsl", "src/fragment.glsl");
	const size_t n_frames = (mode == OUTPUT)? default_frames: sim_repr.header.frame_count;

	GLuint sim;
	glGenTextures(1, &sim);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, sim);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA32F, width, height, n_frames);
	glUseProgram(graphics_shdr);
	glUniform1i(0 /* graphics binding for screen */, 1 /* GL_TEXTURE1 */);

	const auto quad_va = describe_va();

	using namespace std::chrono_literals;
	const std::chrono::milliseconds frame_time{
		(mode == OUTPUT)? default_frame_time: sim_repr.header.ms_per_frame
	};

	if (mode == OUTPUT) {
		auto compute_src = load_file("src/compute.glsl");
		const auto compute_shdr = build_shader(compute_src);
		delete[] compute_src;

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
			glm::vec4 q_orientation;
			glm::vec3 cam_pos;
			float inv_screen_width;
			glm::vec3 sphere_pos;
			float focal_length;
			float sch_radius;
			GLuint iterations;
			unsigned char __padding[8];
		} data;

		data.sphere_pos = glm::vec3{2.0f, 0.0f, -2.0f};
		static constexpr float fov = std::numbers::pi_v<float> / 3.0f;
		data.inv_screen_width = 1.0f / float(width);
		data.focal_length = 0.5f / std::tan(fov * 0.5f);
		data.iterations = default_iterations;

		GLuint ssb;
		glGenBuffers(1, &ssb);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof data, &data, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1 /* binding */, ssb);

		glUseProgram(compute_shdr);
		glUniform1i(2 /* skybox */, 0 /* GL_TEXTURE0 */);

		for (size_t i_frame = 0; win && i_frame < n_frames; ++i_frame) {
			glBindImageTexture(0 /* cs binding */, sim, 0, GL_FALSE, i_frame, GL_WRITE_ONLY, GL_RGBA32F);
			glUseProgram(compute_shdr);
			float progress = float(i_frame) / float(n_frames-1);
			progress = smoothstep(progress);
			float angle = glm::mix(start_angle, end_angle, progress);
			data.q_orientation = glm::vec4{std::sin(angle * 0.5f) * Y, std::cos(angle * 0.5f)};
			data.cam_pos = glm::mix(start_pos, end_pos, progress);
			data.sch_radius = glm::mix(start_sch_r, end_sch_r, progress);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof data, &data);
			glDispatchCompute(width, height, 1);
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
			draw_settings set{graphics_shdr, quad_va, 1, float(i_frame)};
			draw_quad(set);
			win.draw();

			std::printf("\rframe #%zu", i_frame);
			std::fflush(stdout);
		}
		std::printf("\r                 \r");

		sim_repr = dump(width, height, n_frames, frame_time, sim);
		int error = to_file(sim_repr, sim_path);
		assert(!error);
	} else {
		upload(std::move(sim_repr), sim);
	}

	for (back_and_forth counter(n_frames); win; counter.advance()) {
		draw_settings set{graphics_shdr, quad_va, 1, float(counter.cur)};
		draw_quad(set);
		win.draw();
		std::this_thread::sleep_for(frame_time);
	}
}

