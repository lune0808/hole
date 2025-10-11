#include <cassert>
#include <cstdlib>
#include <cstdio>
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

