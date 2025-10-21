#pragma once

#include <aio.h>
#include <chrono>
#include <memory>
#include <cstdint>

struct file
{
	struct request : public aiocb
	{
		bool is_read;

		int fd() const { return aio_fildes; }
		size_t size() const { return aio_nbytes; }
		off_t addr() const { return aio_offset; }
		void *buf() const { return const_cast<void*>(aio_buf); }
	};

	int fd;

	enum class mode { read, write, append };
	file(const char *path, mode mode, size_t buf_elem_count);
	~file();

	request read(void *buf, size_t size, off_t at);
	request write(void *buf, size_t size, off_t at);

	static constexpr std::chrono::milliseconds wait{0};
	bool execute(request req, std::chrono::milliseconds timeout);
	bool cancel(request req);
};

