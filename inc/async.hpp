#pragma once

#include <aio.h>
#include <chrono>

struct file
{
	aiocb ctrl;
	off_t pos;

	enum class mode_t { read, write, append };
	file(const char *path, mode_t mode);
	~file();

	void read(void *buf, size_t size);
	void write(const void *buf, size_t size);
	bool execute(std::chrono::milliseconds timeout);
};

