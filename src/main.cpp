#include <cstdio>
#include <atomic>
#include <fstream>
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

using namespace std::chrono_literals;

static constexpr size_t host_pixel_size = sizeof(std::uint32_t);

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

struct chunk_info_t {
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t frame_count;
	std::uint32_t ms_per_frame;
};

using chunk_t = std::unique_ptr<std::uint32_t[]>;

size_t npixels(chunk_info_t const &h)
{
	return h.width * h.height * h.frame_count;
}

void print_chunk(const std::uint32_t *buf, size_t count)
{
	std::printf("chunk:");
	static constexpr std::uint32_t ff = 0xff;
	for (size_t w = 0; w < count; ++w) {
		std::printf("%c%02x %02x %02x %02x",
			(w % 4 == 0) ? '\n': ' ',
			(buf[w] >>  0) & ff,
			(buf[w] >>  8) & ff,
			(buf[w] >> 16) & ff,
			(buf[w] >> 24) & ff
		);
	}
	std::printf("\n");
}

void color_chunk(void *buf, size_t size, std::uint32_t color)
{
	for (size_t i = 0; i < size/host_pixel_size; ++i) {
		((std::uint32_t*) buf)[i] = color;
	}
}

struct io_request
{
	void *buf;
	size_t size;
	off_t addr;
};

enum class io_work_type { none = 0, open_read, open_write, read, write, exit };
static std::atomic<io_work_type> current_io_work_type;
static io_request current_io_request;
static char *streaming_memory;
static GLsync transfer_fence;
static GLsync draw_fence;

static constexpr auto poll_period = 100ms;

void io_worker(std::atomic<io_work_type> *type)
{
	std::ofstream os;
	std::ifstream is;
	for (;;) {
		auto t = type->load(std::memory_order_acquire);
		auto req = current_io_request;
		auto buf = reinterpret_cast<char*>(req.buf);
		switch (t) {
		case io_work_type::none:
			std::this_thread::sleep_for(poll_period);
			continue;
		case io_work_type::open_write:
			os = std::ofstream(buf);
			assert(os.is_open());
			break;
		case io_work_type::open_read:
			is = std::ifstream(buf);
			assert(is.is_open());
			break;
		case io_work_type::write:
			assert(os.is_open());
			if (!(os.seekp(req.addr) && os.write(buf, req.size))) {
				std::printf("[io error] write\n");
				std::memset(buf, 'W' /* 0x57 */, req.size);
			}
			break;
		case io_work_type::read:
			assert(is.is_open());
			if (!(is.seekg(req.addr) && is.read(buf, req.size))) {
				std::printf("[io error] read\n");
				std::memset(buf, 'R' /* 0x52 */, req.size);
			}
			std::printf(">@%8zx\n", req.addr);
			break;
		case io_work_type::exit:
			return;
		}
		type->store(io_work_type::none, std::memory_order_release);
	}
}

io_request issue_io_request(io_work_type type,
	void *buf = nullptr, size_t size = 0, off_t addr = 0)
{
	io_request req = io_request{buf, size, addr};
	current_io_request = req;
	const auto cur = current_io_work_type.load();
	assert(type == io_work_type::exit || cur == io_work_type::none);
	current_io_work_type.store(type, std::memory_order_release);
	return req;
}

io_request issue_open(io_work_type type, const char *path)
{
	return issue_io_request(type, const_cast<char*>(path));
}

bool io_completed()
{
	return current_io_work_type.load(std::memory_order_acquire) == io_work_type::none;
}

using clk = std::chrono::steady_clock;
using instant_t = std::chrono::time_point<clk>;
using time_interval = std::chrono::nanoseconds;

bool try_complete_io_request(instant_t deadline)
{
	for (;;) {
		const auto now = clk::now();
		if (now >= deadline) {
			return false;
		} else if (io_completed()) {
			return true;
		}
		std::this_thread::sleep_for(poll_period);
	}
}

bool try_complete_io_request(time_interval timeout)
{
	return try_complete_io_request(clk::now() + timeout);
}

void complete_io_request()
{
	const auto status = try_complete_io_request(instant_t::max());
	// assert(status);
}

void complete_dump()
{
	complete_io_request();
}

io_request issue_dump(size_t size, GLuint chunk_name, off_t addr, void *buf)
{
	// this blocks until packing is done, also the floating point format is converted
	// ideally we would keep floats and stream the texture using DSA
	glGetTextureImage(chunk_name, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, size, buf);
	return issue_io_request(io_work_type::write, buf, size, addr);
}

io_request issue_load(void *buf, size_t size, off_t addr)
{
	std::printf("<@%8zx -> %p\n", addr, buf);
	return issue_io_request(io_work_type::read, buf, size, addr);
}

io_request blocking_load(void *buf, size_t size, off_t addr)
{
	auto req = issue_load(buf, size, addr);
	complete_io_request();
	return req;
}

bool fence_try_wait(GLsync fence, time_interval timeout)
{
	const auto status = glClientWaitSync(fence, 0, timeout.count());
	bool ok = (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED);
	if (ok) {
		std::printf(">fence\n");
	}
	return ok;
}

bool fence_try_wait(GLsync fence, instant_t deadline)
{
	return fence_try_wait(fence, deadline - clk::now());
}

bool fence_block(GLsync fence)
{
	glFlush();
	return fence_try_wait(fence, time_interval::max());
}

bool pixel_unpack(GLuint name, chunk_info_t const &info, GLintptr device_addr)
{
	std::printf("<             %p = pinned[%7tx] -> T%u\n",
		streaming_memory + device_addr, device_addr, name);
	// TODO: change format to floats
	glTextureSubImage3D(name, 0, 0, 0, 0, info.width, info.height, info.frame_count,
		GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, (void*) device_addr);
	glDeleteSync(transfer_fence);
	transfer_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	return true;
}

// we can't unpack to texture[i+2] while
// we are drawing texture[i], so instead
// texture[i] finishes whatever the data
// pipeline is for i+1 and then begins
// loading for the i+2 pipeline.
// so chunk_name, device_addr and
// transfer_fence refer to the current
// buffer's next, and the io_request
// refer to the current buffer's next's next.
// when the time comes to draw i+1,
// this `pipeline` gets reset so it
// must be flushed prior.
int try_stream_load(io_request req, chunk_info_t const &info,
	GLintptr device_addr, GLuint chunk_name, int suspend, instant_t deadline)
{
	// coroutine lol
	switch (suspend) {
	case 4:
		if (!pixel_unpack(chunk_name, info, device_addr)) {
			return 4;
		}
		/* fallthrough */
	case 3:
		if (!fence_try_wait(transfer_fence, deadline)) {
			return 3;
		}
		/* fallthrough */
	case 2:
		issue_load(req.buf, req.size, req.addr);
		/* fallthrough */
	case 1:
		if (!try_complete_io_request(deadline)) {
			return 1;
		}
		/* fallthrough */
	case 0:
		return 0;
	default:
		assert(false && "oob suspend value");
	}
}

bool force_stream_load(io_request next_req, chunk_info_t const &info, GLintptr device_addr, GLuint chunk_name, int suspend)
{
	return try_stream_load(next_req, info, device_addr, chunk_name, suspend, instant_t::max());
}

struct draw_settings {
	GLuint shader;
	GLuint quad_va;
	GLuint frame_location;
	float frame_value;
	GLuint screen_location;
	GLuint screen_value;
};

void draw_quad(draw_settings set)
{
	glUseProgram(set.shader);
	glUniform1f(set.frame_location, set.frame_value);
	glUniform1i(set.screen_location, set.screen_value);
	glBindVertexArray(set.quad_va);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

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
static constexpr size_t default_frames = 128;
static constexpr size_t default_frame_time = 5000 / default_frames;
static constexpr GLuint compute_local_dim = 4;
static constexpr size_t default_iterations = 48;
static constexpr size_t chunk_frame_count = 16;

GLuint load_skybox(GLenum unit, const char *path_fmt)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	const char *replacement[6] = { "right", "left", "top", "bottom", "front", "back" };
	static constexpr size_t bufsize = 128;
	char buf[bufsize];
	for (size_t face = 0; face < std::size(replacement); ++face) {
		int width;
		int height;
		int channels;
		std::snprintf(buf, bufsize, path_fmt, replacement[face]);
		auto data = stbi_load(buf, &width, &height, &channels, 4);
		assert(data);
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
			0 /* mipmap */, GL_RGBA,
			width, height,
			0, GL_RGBA, GL_UNSIGNED_BYTE, data
		);
		stbi_image_free(data);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	return tex;
}

GLuint texture_array(GLenum unit, GLenum format, size_t width, size_t height, size_t depth)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1 /* mipmap */, format, width, height, depth);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	return tex;
}

void enable_sim_frame(GLuint binding, GLuint texture, size_t index, GLenum format)
{
	glBindImageTexture(binding, texture, 0, GL_FALSE, index, GL_WRITE_ONLY, format);
}

GLuint back_and_forth(GLuint index_, GLuint max_value_)
{
	int index = index_;
	int max_value = max_value_;
	return max_value - std::abs(index % (2 * max_value) - max_value);
}

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

	chunk_info_t sim_repr;
	if (mode == INPUT) {
		std::ifstream input{sim_path};
		if (!input.read(reinterpret_cast<char*>(&sim_repr), sizeof sim_repr)) {
			return 1;
		}
	} else {
		sim_repr.width = default_width;
		sim_repr.height = default_height;
		sim_repr.frame_count = default_frames;
		sim_repr.ms_per_frame = default_frame_time;
	}
	const int width = sim_repr.width;
	const int height = sim_repr.height;
	const size_t n_frames = sim_repr.frame_count;
	const std::chrono::milliseconds frame_time{sim_repr.ms_per_frame};
	glfw_context glfw{};
	window win(width, height, glfw);
	const auto graphics_shdr = graphics_shader("src/vertex.glsl", "src/fragment.glsl");

	GLuint sim[2];
	sim[0] = texture_array(GL_TEXTURE0, GL_RGBA32F, width, height, chunk_frame_count);
	if (n_frames > chunk_frame_count) {
		sim[1] = texture_array(GL_TEXTURE1, GL_RGBA32F, width, height, chunk_frame_count);
	}

	const auto quad_va = describe_va();
	std::jthread io(io_worker, &current_io_work_type);

	assert(sim_repr.frame_count % chunk_frame_count == 0);
	assert(sim_repr.frame_count > chunk_frame_count);

	if (mode == OUTPUT) {
		auto wreq = issue_open(io_work_type::open_write, sim_path);
		complete_io_request();
		wreq = issue_io_request(io_work_type::write, &sim_repr, sizeof sim_repr, 0);
		off_t write_addr = sizeof sim_repr;

		auto compute_src = load_file("src/compute.glsl");
		const auto compute_shdr = build_shader(compute_src);
		delete[] compute_src;

		GLuint skybox = load_skybox(GL_TEXTURE2, "res/sky_%s.png");

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

		glProgramUniform1i(compute_shdr, 2 /* skybox */, 2 /* GL_TEXTURE2 */);

		const GLuint compute_width = (width + compute_local_dim - 1) / compute_local_dim;
		const GLuint compute_height = (height + compute_local_dim - 1) / compute_local_dim;
		const size_t chunk_pixels = width * height * chunk_frame_count;
		const size_t chunk_size = chunk_pixels * host_pixel_size;
		auto buf = std::make_unique<std::uint32_t[]>(chunk_pixels);
		// n_frames should always be a multiple of chunk_frame_count
		for (size_t i_frame = 0; win && i_frame < n_frames; ++i_frame) {
			const GLuint buffer = (i_frame / chunk_frame_count) % 2;
			const GLuint frame_index = i_frame % chunk_frame_count;
			float progress = smoothstep(float(i_frame) / float(n_frames-1));
			const float angle = glm::mix(start_angle, end_angle, progress);
			data.q_orientation = glm::vec4{std::sin(angle * 0.5f) * Y, std::cos(angle * 0.5f)};
			data.cam_pos = glm::mix(start_pos, end_pos, progress);
			data.sch_radius = glm::mix(start_sch_r, end_sch_r, progress);

			glUseProgram(compute_shdr);
			enable_sim_frame(0, sim[buffer], frame_index, GL_RGBA32F);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof data, &data);
			glDispatchCompute(compute_width, compute_height, 1);
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

			draw_settings set{
				graphics_shdr, quad_va,
				1, float(frame_index),
				0, buffer,
			};
			draw_quad(set);
			win.draw();

			if (frame_index == chunk_frame_count-1) {
				// technically this only needs to wait for the request
				// that was made for the previous issue with the same
				// `buffer` index
				complete_dump();
				wreq = issue_dump(chunk_size, sim[buffer], write_addr, buf.get());
				write_addr += chunk_size;
			}

			std::printf("\rframe #%zu", i_frame);
			std::fflush(stdout);
		}
		std::printf("\r                 \r");
		// push the last issue
		complete_dump();
	}

	if (win) {
		issue_open(io_work_type::open_read, sim_path);
		complete_io_request();
		blocking_load(&sim_repr, sizeof sim_repr, 0);
		chunk_info_t chunk{
			sim_repr.width, sim_repr.height, chunk_frame_count, sim_repr.ms_per_frame
		};
		const size_t chunk_pixels = npixels(chunk);
		const size_t chunk_size = chunk_pixels * sizeof(std::uint32_t);
		const size_t chunk_count = sim_repr.frame_count / chunk_frame_count;

		GLuint pixel_transfer;
		glGenBuffers(1, &pixel_transfer);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_transfer);
		// TODO: replace GL_MAP_COHERENT_BIT by GL_MAP_UNSYNCHRONIZED_BIT
		// because of the double buffering
		GLbitfield pixel_transfer_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glBufferStorage(GL_PIXEL_UNPACK_BUFFER, 2 * chunk_size, nullptr, pixel_transfer_flags);
		streaming_memory = (char*) glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 2 * chunk_size, pixel_transfer_flags);

		assert(sim_repr.frame_count >= 5*chunk_frame_count);
		std::printf("A\n");
		blocking_load(streaming_memory, 2*chunk_size, sizeof sim_repr);
		std::printf("B\n");
		pixel_unpack(sim[0], chunk, 0);
		pixel_unpack(sim[1], chunk, chunk_size);
		fence_block(transfer_fence);
		// rreq is made as if it produced the current state
		auto rreq = blocking_load(streaming_memory, chunk_size, sizeof sim_repr + 2*chunk_size);
		std::printf("buffers primed\n");

		int upload_state = 0;
		GLuint prev_chunk_index = 0;
		size_t device_addr = chunk_size;
		for (GLuint frame = 0; win; ++frame) {
			const auto frame_start_time = clk::now();
			const GLuint frame_index = back_and_forth(frame, sim_repr.frame_count-1);
			const GLuint chunk_index = frame_index / chunk_frame_count;
			const GLuint buffer = chunk_index % 2;
			const GLuint next_buffer = buffer ^ 1;
			const GLuint buffer_index = frame_index % chunk_frame_count;
			const GLuint next_chunk_frame_index = back_and_forth(frame + 2*chunk_frame_count-1, sim_repr.frame_count-1);
			const GLuint next_chunk_index = next_chunk_frame_index / chunk_frame_count;
			if (chunk_index != prev_chunk_index) {
				if (upload_state != 0) {
					force_stream_load(rreq, chunk, device_addr, sim[buffer], upload_state);
				}
				std::printf("chunk swap (%sforced)\n", (upload_state != 0) ? "": "not ");
				const GLuint next_next_chunk_index =
					back_and_forth(frame + 3*chunk_frame_count-1, sim_repr.frame_count-1) / chunk_frame_count;
				upload_state = 4;
				device_addr = next_buffer * chunk_size;
				rreq.buf = streaming_memory + buffer * chunk_size;
				rreq.addr = sizeof sim_repr + next_next_chunk_index * chunk_size;
				prev_chunk_index = chunk_index;
			} else {
				const auto deadline = frame_start_time + frame_time/2;
				const auto new_state = try_stream_load(rreq, chunk, device_addr,
					sim[next_buffer], upload_state, deadline);
				if (new_state != upload_state) {
					std::printf("frame %u progressed %d->%d\n", buffer_index, upload_state, new_state);
				}
				upload_state = new_state;
			}

			std::printf("draw T%u\n", sim[buffer]);
			draw_settings set{
				graphics_shdr, quad_va,
					1, float(buffer_index),
					0, buffer,
			};
			draw_quad(set);
			win.draw();
			std::this_thread::sleep_until(frame_start_time + frame_time);
		}
	}

	complete_io_request();
	issue_io_request(io_work_type::exit);
}

