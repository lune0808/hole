#pragma once

#include <aio.h>
#include <chrono>
#include <memory>
#include <cstdint>

struct file
{
	aiocb ctrl;
	off_t pos;
	std::unique_ptr<std::uint32_t[]> buf;

	enum class mode_t { read, write, append };
	file(const char *path, mode_t mode, size_t buf_elem_count);
	~file();

	void read(size_t size);
	void write(size_t size);

	static constexpr std::chrono::milliseconds wait{0};
	bool execute(std::chrono::milliseconds timeout);
	bool cancel();
};

