#include "std.hpp"
#include "shader.hpp"


static void compile_test(GLuint id)
{
	int success;
	glGetShaderiv(id, GL_COMPILE_STATUS, &success);
	if (success) return;
	GLchar buf[1024];
	glGetShaderInfoLog(id, sizeof buf, nullptr, buf);
	std::fprintf(stderr, "[gl compile error] %s\n", buf);
	std::exit(1);
}

static void link_test(GLuint id)
{
	int success;
	glGetProgramiv(id, GL_LINK_STATUS, &success);
	if (success) return;
	GLchar buf[1024];
	glGetProgramInfoLog(id, sizeof buf, nullptr, buf);
	std::fprintf(stderr, "[gl link error] %s\n", buf);
	std::exit(1);
}

GLuint compile_shader(const char *src, GLenum kind)
{
	const auto id = glCreateShader(kind);
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);
	compile_test(id);
	return id;
}

GLuint build_shader(const char *vert_src, const char *frag_src)
{
	const auto id = glCreateProgram();
	const auto vert = compile_shader(vert_src, GL_VERTEX_SHADER);
	const auto frag = compile_shader(frag_src, GL_FRAGMENT_SHADER);
	glAttachShader(id, vert);
	glAttachShader(id, frag);
	glLinkProgram(id);
	link_test(id);
	glDetachShader(id, vert);
	glDetachShader(id, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);
	return id;
}

GLuint build_shader(const char *comp_src)
{
	const auto id = glCreateProgram();
	const auto comp = compile_shader(comp_src, GL_COMPUTE_SHADER);
	glAttachShader(id, comp);
	glLinkProgram(id);
	link_test(id);
	glDetachShader(id, comp);
	glDeleteShader(comp);
	return id;
}

size_t file_size(const char *path)
{
	std::ifstream file(path);
	file.seekg(0, std::ios_base::end);
	return size_t(file.tellg()) + 1u /* for terminator */;
}

size_t load_file(const char *path, char *buf, size_t size, char terminator)
{
	std::ifstream file(path);
	if (size > (1ul << 20)) {
		std::fprintf(stderr, "tried to load '%s' (%zuMiB) as shader code.\n", path, size >> 20);
		std::exit(1);
	}
	file.read(buf, size - 1);
	buf[size - 1] = terminator;
	return size;
}

size_t load_file(const char *path, std::span<char> buf, char terminator)
{
	return load_file(path, buf.data(), buf.size(), terminator);
}
