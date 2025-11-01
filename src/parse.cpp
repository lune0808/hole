#include "parse.hpp"
#include "shader.hpp"


command_line parse_command_line(int argc, char **argv)
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

shaders_text_blob load_all_shaders(const char *vs, const char *fs, const char *cs, const char *sc)
{
	shaders_text_blob sh;
	const auto vs_sz = file_size(vs);
	const auto fs_sz = file_size(fs);
	const auto cs_sz = file_size(cs); // includes src/shared_data.glsl
	const auto sc_sz = file_size(sc); // includes src/shared_data.glsl, inc/skybox_id.hpp, src/script_include.glsl
	constexpr const char *shared = "src/shared_data.glsl";
	const auto shared_sz = file_size(shared);
	constexpr const char *skybox_id = "inc/skybox_id.hpp";
	const auto skybox_id_sz = file_size(skybox_id);
	constexpr const char *include = "src/script_include.glsl";
	const auto include_sz = file_size(include);
	constexpr const char *include_main = "src/script_include_main.glsl";
	const auto include_main_sz = file_size(include_main);
	sh.memory = std::make_unique<char[]>(
		+ vs_sz
		+ fs_sz
		+ shared_sz + cs_sz
		+ shared_sz + skybox_id_sz + include_sz + sc_sz + include_main_sz
	);

	char *at = sh.memory.get();
	sh.quad_vs = std::span{at, vs_sz};
	at += load_file(vs, at, vs_sz, '\0');
	sh.quad_fs = std::span{at, fs_sz};
	at += load_file(fs, at, fs_sz, '\0');
	sh.sim_cs = std::span{at, shared_sz + cs_sz};
	at += load_file(shared, at, shared_sz, '\n');
	at += load_file(cs, at, cs_sz, '\0');
	sh.script = std::span{at, shared_sz + skybox_id_sz
		+ include_sz + sc_sz + include_main_sz};
	std::memcpy(at, sh.sim_cs.data(), shared_sz);
	at += shared_sz;
	at += load_file(skybox_id, at, skybox_id_sz, '\n');
	at += load_file(include, at, include_sz, '\n');
	at += load_file(sc, at, sc_sz, '\n');
	at += load_file(include_main, at, include_main_sz, '\0');
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

