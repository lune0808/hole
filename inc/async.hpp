#pragma once

#include <aio.h>
#include <chrono>
#include <memory>
#include <cstdint>

struct file
{
	aiocb ctrl;
	std::unique_ptr<std::uint32_t[]> buf;

	enum class mode { read, write, append };
	file(const char *path, mode mode, size_t buf_elem_count);
	~file();

	off_t read(size_t size, off_t at);
	off_t write(size_t size, off_t at);

	static constexpr std::chrono::milliseconds wait{0};
	bool execute(std::chrono::milliseconds timeout);
	bool cancel();
};

