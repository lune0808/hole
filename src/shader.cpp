#include <cassert>
#include <cstdlib>
#include <cstdio>
#include "shader.hpp"


GLuint compile_shader(const char *src, GLenum kind)
{
	const auto id = glCreateShader(kind);
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);
	int success;
	glGetShaderiv(id, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar buf[1024];
		GLsizei len;
		glGetShaderInfoLog(id, sizeof buf, &len, buf);
		std::fprintf(stderr, "[gl compile error] %s\n", buf);
		std::exit(1);
	}
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
	glDeleteShader(comp);
	return id;
}

