#pragma once

#include "std.hpp"


enum cmd_type { OUTPUT, INPUT, RECOVER };

struct command_line {
	const char *sim_path;
	const char *script_path;
	cmd_type mode;
};

command_line parse_command_line(int argc, char **argv);

struct shaders_text_blob
{
	using view = std::span<char>;
	std::unique_ptr<char[]> memory;
	view quad_vs;
	view quad_fs;
	view sim_cs;
	view script;
};

shaders_text_blob load_all_shaders(const char *vs, const char *fs,
	const char *cs, const char *sc);
shaders_text_blob load_draw_shaders(const char *vs, const char *fs);

