#include "std.hpp"
#include "window.hpp"
#include "shader.hpp"
#include "skybox_id.hpp"
#include "gl_object.hpp"
#include "timing.hpp"
#include "io.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

using namespace std::chrono_literals;

static constexpr GLuint compute_local_dim = 4;
static constexpr size_t chunk_frame_count = 16;
static constexpr size_t host_pixel_size = sizeof(std::uint32_t);

static const float quad[] = {
	-1.0f, -1.0f, 0.0f, 1.0f,
	+1.0f, -1.0f, 0.0f, 1.0f,
	-1.0f, +1.0f, 0.0f, 1.0f,
	-1.0f, +1.0f, 0.0f, 1.0f,
	+1.0f, -1.0f, 0.0f, 1.0f,
	+1.0f, +1.0f, 0.0f, 1.0f,
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
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
	return va;
}

static float smoothstep(float x)
{
	// flat tangents at 0 & 1, mapping [0,1] to [0,1]
	return 3.0f * x * x - 2.0f * x * x * x;
}

struct file_header_t {
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t frame_count;
	std::uint32_t ms_per_frame;
};

static GLsync transfer_fence;

void complete_dump()
{
	complete_io_request();
}

void issue_dump(size_t size, GLuint chunk_name, off_t addr, void *buf)
{
	// this blocks until packing is done, also the floating point format is converted
	// ideally we would keep floats and stream the texture using DSA
	glGetTextureImage(chunk_name, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, size, buf);
	issue_io_request(io_work_type::write, buf, size, addr);
}

bool issue_load(void *buf, size_t size, off_t addr)
{
	return issue_io_request(io_work_type::read, buf, size, addr);
}

void blocking_load(void *buf, size_t size, off_t addr)
{
	issue_load(buf, size, addr);
	complete_io_request();
}

void pixel_unpack(GLuint name, GLuint width, GLuint height, GLintptr device_addr)
{
	glTextureSubImage3D(name, 0, 0, 0, 0, width, height, chunk_frame_count,
		GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, (void*) device_addr);
	transfer_fence = fence_insert(transfer_fence);
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
static constexpr int try_stream_load_reset = 4;
static constexpr int try_stream_load_nop = 0;
int try_stream_load(io_request req, GLuint width, GLuint height,
	GLintptr device_addr, GLuint chunk_name, int suspend, instant_t deadline)
{
	// coroutine lol
	switch (suspend) {
	case 4:
		pixel_unpack(chunk_name, width, height, device_addr);
		/* fallthrough */
	case 3:
		if (!fence_try_wait(transfer_fence, deadline)) {
			return 3;
		}
		/* fallthrough */
	case 2:
		if (!issue_load(req.buf, req.size, req.addr)) {
			return 2;
		}
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

void draw_quad(GLuint shader, GLuint quad_va, float frame, float select)
{
	glUseProgram(shader);
	glUniform1f(2, frame);
	glUniform1f(3, select);
	glBindVertexArray(quad_va);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

enum texture_settings {
	use_mipmaps = 1 << 0,
};

void sensible_texture_defaults(GLenum kind, texture_settings ts = texture_settings(0))
{
	glTexParameteri(kind, GL_TEXTURE_MIN_FILTER,
		(ts & texture_settings::use_mipmaps)? GL_LINEAR_MIPMAP_LINEAR: GL_LINEAR);
	glTexParameteri(kind, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(kind, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(kind, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(kind, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	if (ts & texture_settings::use_mipmaps) {
		glGenerateMipmap(kind);
	}
}

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
	sensible_texture_defaults(GL_TEXTURE_CUBE_MAP, texture_settings::use_mipmaps);
	return tex;
}

GLuint texture_array(GLenum unit, GLenum format, size_t width, size_t height, size_t depth)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1 /* mipmap */, format, width, height, depth);
	sensible_texture_defaults(GL_TEXTURE_2D_ARRAY);
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

struct shaders_text_blob
{
	using buffer = std::span<char>;
	std::unique_ptr<char[]> memory;
	buffer quad_vs;
	buffer quad_fs;
	buffer sim_cs;
	buffer script;
};

shaders_text_blob load_all_shaders(const char *vs, const char *fs, const char *cs, const char *sc)
{
	shaders_text_blob sh;
	const auto vs_sz = file_size(vs);
	const auto fs_sz = file_size(fs);
	const auto cs_sz = file_size(cs); // includes src/shared_data.glsl
	const auto sc_sz = file_size(sc); // includes src/shared_data.glsl, inc/skybox_id.hpp, src/script_include.glsl
	static constexpr const char *const shared = "src/shared_data.glsl";
	const auto shared_sz = file_size(shared);
	static constexpr const char *const skybox_id = "inc/skybox_id.hpp";
	const auto skybox_id_sz = file_size(skybox_id);
	static constexpr const char *const include = "src/script_include.glsl";
	const auto include_sz = file_size(include);
	static constexpr const char *const include_main = "src/script_include_main.glsl";
	const auto include_main_sz = file_size(include_main);
	sh.memory = std::make_unique<char[]>(
		+ vs_sz
		+ fs_sz
		+ shared_sz + cs_sz
		+ shared_sz + skybox_id_sz + include_sz + sc_sz + include_main_sz
	);
	sh.quad_vs = std::span{sh.memory.get(), vs_sz};
	sh.quad_fs = std::span{sh.quad_vs.data() + sh.quad_vs.size(), fs_sz};
	sh.sim_cs = std::span{sh.quad_fs.data() + sh.quad_fs.size(), shared_sz + cs_sz};
	sh.script = std::span{sh.sim_cs.data() + sh.sim_cs.size(), shared_sz + skybox_id_sz + include_sz + sc_sz + include_main_sz};
	load_file(vs, sh.quad_vs, '\0');
	load_file(fs, sh.quad_fs, '\0');
	load_file(shared, sh.sim_cs.data(), shared_sz, '\n');
	load_file(cs, sh.sim_cs.data() + shared_sz, cs_sz, '\0');
	std::memcpy(sh.script.data(), sh.sim_cs.data(), shared_sz);
	load_file(skybox_id, sh.script.data() + shared_sz, skybox_id_sz, '\n');
	load_file(include, sh.script.data() + shared_sz + skybox_id_sz, include_sz, '\n');
	load_file(sc, sh.script.data() + shared_sz + skybox_id_sz + include_sz, sc_sz, '\n');
	load_file(include_main, sh.script.data() + shared_sz + skybox_id_sz + include_sz + sc_sz, include_main_sz, '\0');
	return sh;
}

shaders_text_blob load_draw_shaders(const char *vs, const char *fs)
{
	shaders_text_blob sh;
	const auto vs_sz = file_size(vs);
	const auto fs_sz = file_size(fs);
	sh.memory = std::make_unique<char[]>(vs_sz + fs_sz);
	sh.quad_vs = std::span{sh.memory.get(), vs_sz};
	sh.quad_fs = std::span{sh.quad_vs.data() + sh.quad_vs.size(), fs_sz};
	load_file(vs, sh.quad_vs, '\0');
	load_file(fs, sh.quad_fs, '\0');
	return sh;
}

enum cmd_type { OUTPUT, INPUT, RECOVER };

struct command_line {
	const char *sim_path;
	const char *script_path;
	cmd_type mode;
};

command_line parse(int argc, char **argv)
{
	static constexpr const char *const default_sim_path = "/tmp/black_hole_sim_data.rgbf32";
	command_line cl;
	if (argc == 3 && std::strcmp(argv[1], "-i") == 0) {
		cl.mode = INPUT;
		cl.sim_path = argv[2];
	} else if (argc == 2) {
		cl.mode = OUTPUT;
		cl.sim_path = default_sim_path;
		cl.script_path = argv[1];
	} else if (argc == 4) {
		cl.script_path = argv[1];
		cl.sim_path = argv[3];
		if (std::strcmp(argv[2], "-r") == 0) {
			cl.mode = RECOVER;
		} else if (std::strcmp(argv[2], "-o") == 0) {
			cl.mode = OUTPUT;
		} else {
			goto usage;
		}
	} else if (argc == 6) {
		if (std::strcmp(argv[2], "-r") != 0 || std::strcmp(argv[4], "-o") != 0) {
			goto usage;
		}
		cl.mode = RECOVER;
		cl.sim_path = argv[5];
		cl.script_path = argv[1];
	} else {
	usage:
		std::printf("usage:\n");
		std::printf("%s <script>.glsl [-r <partial-file>] [-o <output-file>]\n", argv[0]);
		std::printf("%s -i <input-file>\n", argv[0]);
		std::exit(1);
	}
	return cl;
}

char *map_persistent_buffer(GLenum target, GLenum access, size_t size)
{
	glBufferStorage(target, size, nullptr, GL_MAP_PERSISTENT_BIT | access);
	return (char*) glMapBufferRange(target, 0, size, GL_MAP_PERSISTENT_BIT | GL_MAP_UNSYNCHRONIZED_BIT | access);
}

int main(int argc, char **argv)
{
	auto cmd = parse(argc, argv);
	off_t recover_chunk = 0;
	file_header_t sim_repr;
	if (cmd.mode == INPUT || cmd.mode == RECOVER) {
		std::ifstream input{cmd.sim_path};
		if (!input.read(reinterpret_cast<char*>(&sim_repr), sizeof sim_repr)) {
			return 1;
		}
		if (cmd.mode == RECOVER) {
			input.seekg(0, std::ios_base::end);
			const size_t size = input.tellg();
			const auto chunk_size = sim_repr.width * sim_repr.height * chunk_frame_count * host_pixel_size;
			// off_t is signed
			recover_chunk = (size - sizeof sim_repr) / chunk_size - 1;
			if (recover_chunk <= 0) {
				recover_chunk = 0;
				cmd.mode = OUTPUT;
			}
		}
	}
	glfw_context glfw{};
	window win(0, 0, glfw);
	GLuint graphics_shdr;
	GLuint compute_shdr;
	GLuint script;
	if (cmd.mode == OUTPUT || cmd.mode == RECOVER) {
		const auto sh_text = load_all_shaders("src/vertex.glsl", "src/fragment.glsl", "src/compute.glsl", cmd.script_path);
		graphics_shdr = build_shader(sh_text.quad_vs.data(), sh_text.quad_fs.data());
		compute_shdr = build_shader(sh_text.sim_cs.data());
		script = build_shader(sh_text.script.data());
	} else if (cmd.mode == INPUT) {
		const auto sh_text = load_draw_shaders("src/vertex.glsl", "src/fragment.glsl");
		graphics_shdr = build_shader(sh_text.quad_vs.data(), sh_text.quad_fs.data());
	}

	const auto quad_va = describe_va();
	io_init();

	if (cmd.mode == OUTPUT || cmd.mode == RECOVER) {
		gl_ssb scene_state{1, 9*sizeof(float[4])};
		gl_ssb scene_settings{0, (2*4 + 2) * sizeof(float[4])};

		struct {
			GLint width;
			GLint height;
			GLuint n_frames;
			GLuint ms_per_frame;
			GLuint skybox_id;
			float fov;
		} window_settings;
		glUseProgram(script);
		glUniform1f(2 /* progress */, -1.0f);
		glDispatchCompute(1, 1, 1);
		glFinish();

		scene_settings.read(&window_settings, 2*4 * sizeof(float[4]), sizeof window_settings);
		assert(window_settings.width > 0 && window_settings.height > 0);
		win.resize(window_settings.width, window_settings.height);
		assert(window_settings.skybox_id < std::size(skybox_fmt));
		const GLuint skybox = load_skybox(GL_TEXTURE2, skybox_fmt[window_settings.skybox_id]);
		const size_t width = sim_repr.width = window_settings.width;
		const size_t height = sim_repr.height = window_settings.height;
		const size_t n_frames = sim_repr.frame_count = window_settings.n_frames;
		sim_repr.ms_per_frame = window_settings.ms_per_frame;

		if (cmd.mode == OUTPUT) {
			blocking_open_trunc(cmd.sim_path);
		} else {
			blocking_open_recover(cmd.sim_path);
		}
		issue_io_request(io_work_type::write, &sim_repr, sizeof sim_repr, 0);
		const size_t chunk_pixels = width * height * chunk_frame_count;
		const size_t chunk_size = chunk_pixels * host_pixel_size;
		off_t write_addr = sizeof sim_repr + recover_chunk * chunk_size;

		GLuint sim;
		sim = texture_array(GL_TEXTURE0, GL_R11F_G11F_B10F, width, height, chunk_frame_count);

		glProgramUniform1i(compute_shdr, 2 /* skybox */, 2 /* GL_TEXTURE2 */);
		glProgramUniform1i(graphics_shdr, 0 /* screen0 */, 0);
		glProgramUniform1i(graphics_shdr, 1 /* screen1 */, 1);

		GLuint compute_width = (width + compute_local_dim - 1) / compute_local_dim;
		GLuint compute_height = (height + compute_local_dim - 1) / compute_local_dim;
		auto buf = std::make_unique<std::uint32_t[]>(chunk_pixels);
		// n_frames should always be a multiple of chunk_frame_count
		for (size_t i_frame = recover_chunk * chunk_frame_count; win && i_frame < n_frames; ++i_frame) {
			// const GLuint buffer = (i_frame / chunk_frame_count) % 2;
			const GLuint frame_index = i_frame % chunk_frame_count;
			float progress = smoothstep(float(i_frame) / float(n_frames-1));

			glUseProgram(script);
			glUniform1f(2 /* progress */, progress);
			glDispatchCompute(1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			auto time_ref = clk::now();
			for (GLint px_base_x = 0; px_base_x < width && win; px_base_x += compute_width * compute_local_dim) {
				for (GLint px_base_y = 0; px_base_y < width && win; px_base_y += compute_height * compute_local_dim) {
					glUseProgram(compute_shdr);
					glUniform2i(3 /* px_base */, px_base_x, px_base_y);
					enable_sim_frame(0, sim, frame_index, GL_R11F_G11F_B10F);
					glDispatchCompute(compute_width, compute_height, 1);
					glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

					draw_quad(graphics_shdr, quad_va, float(frame_index), float(0.0f));
					win.present();
					std::printf("\rframe %zu/%zu:%02zu%%",
						i_frame+1, n_frames, (100 * (px_base_x * height + px_base_y)) / (width * height));
					std::fflush(stdout);
					const auto time_test = clk::now();
					const auto elapsed = time_test - time_ref;
					if (elapsed > 100ms && compute_width > 16 && compute_height > 16) {
						compute_width /= 2;
						compute_height /= 2;
					}
					time_ref = time_test;
				}
			}

			if (frame_index == chunk_frame_count-1) {
				// technically this only needs to wait for the request
				// that was made for the previous issue with the same
				// `buffer` index
				complete_dump();
				issue_dump(chunk_size, sim, write_addr, buf.get());
				write_addr += chunk_size;
			}
		}
		std::printf("\r                 \r");
		// push the last issue
		complete_dump();
		blocking_close();
		glDeleteTextures(1, &sim);
	}

	assert(sim_repr.frame_count % chunk_frame_count == 0);
	assert(sim_repr.frame_count > chunk_frame_count);

	const auto width = sim_repr.width;
	const auto height = sim_repr.height;
	const auto n_frames = sim_repr.frame_count;
	GLuint sim[2];
	sim[0] = texture_array(GL_TEXTURE0, GL_R11F_G11F_B10F, width, height, chunk_frame_count);
	sim[1] = texture_array(GL_TEXTURE1, GL_R11F_G11F_B10F, width, height, chunk_frame_count);

	if (win) {
		win.resize(width, height);
		blocking_open_read(cmd.sim_path);
		const size_t chunk_pixels = width * height * chunk_frame_count;
		const size_t chunk_size = chunk_pixels * sizeof(std::uint32_t);
		const off_t chunk_count = n_frames / chunk_frame_count;

		GLuint pixel_transfer;
		glGenBuffers(1, &pixel_transfer);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_transfer);
		// TODO: fix screen tearing
		char *streaming_memory =
			map_persistent_buffer(GL_PIXEL_UNPACK_BUFFER, GL_MAP_WRITE_BIT, 2 * chunk_size);

		off_t video_file_offset = sizeof sim_repr;
		assert(n_frames >= 2*chunk_frame_count);
		blocking_load(streaming_memory, 2*chunk_size, video_file_offset);
		pixel_unpack(sim[0], width, height, 0);
		pixel_unpack(sim[1], width, height, chunk_size);
		fence_block(transfer_fence);
		// rreq is made as if it produced the current state
		const auto ichunksize = off_t(chunk_size);
		blocking_load(streaming_memory, chunk_size, video_file_offset + (2%chunk_count)*ichunksize);
		io_request rreq{streaming_memory, chunk_size, video_file_offset + (2%chunk_count)*ichunksize};

		auto upload_state = try_stream_load_nop;
		GLuint prev_chunk = 0;
		size_t device_addr = chunk_size;
		const auto start_time = clk::now();
		const std::chrono::milliseconds frame_time{sim_repr.ms_per_frame};
		for (GLuint present_frame = 0; win; ++present_frame) {
			const GLuint anim_frame = back_and_forth(present_frame, n_frames - 1);
			const GLuint chunk = anim_frame / chunk_frame_count;
			const GLuint buffer = chunk % 2;
			const GLuint next_buffer = buffer ^ 1;
			const GLuint buffer_index = anim_frame % chunk_frame_count;
			const auto frame_start_time = start_time + (present_frame-1) * frame_time;
			const auto deadline = frame_start_time + frame_time/2;
			const auto old = upload_state;
			upload_state = try_stream_load(rreq, width, height, device_addr,
				sim[next_buffer], upload_state, deadline);
			if (chunk != prev_chunk) {
				const GLuint loading_frame = back_and_forth(
					present_frame + 3*chunk_frame_count-1,
					n_frames - 1
				);
				const GLuint loading_chunk = loading_frame / chunk_frame_count;
				upload_state = try_stream_load_reset;
				device_addr = next_buffer * chunk_size;
				rreq.buf = streaming_memory + buffer * chunk_size;
				rreq.addr = video_file_offset + loading_chunk * chunk_size;
				prev_chunk = chunk;
			}

			draw_quad(graphics_shdr, quad_va, float(buffer_index), float(buffer));
			win.present();
			std::this_thread::sleep_until(start_time + present_frame * frame_time);
		}
		blocking_close();
		glDeleteTextures(2, sim);
		glDeleteBuffers(1, &pixel_transfer);
	}

	io_fini();
}

