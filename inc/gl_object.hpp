#pragma once

#include "std.hpp"
#include "libs.hpp"

struct gl_ssb
{
	GLuint name;
	GLuint binding;
	std::size_t size;

	gl_ssb(GLuint binding, std::size_t size)
		: binding{binding}, size{size}
	{
		glCreateBuffers(1, &name);
		glNamedBufferStorage(name, size, nullptr, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, name);
	}

	~gl_ssb()
	{
		glDeleteBuffers(1, &name);
	}

	void read(void *buf, std::size_t offset, std::size_t size)
	{
		glGetNamedBufferSubData(name, offset, size, buf);
	}
};

struct gl_buffer
{
	GLuint name;
	std::size_t size;
};

