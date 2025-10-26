#pragma once

#include <chrono>
#include <atomic>
#include "timing.hpp"


enum class io_work_type {
	none = 0,
	open_read,
	open_write,
	open_recover,
	close,
	read,
	write,
	exit
};

struct io_request
{
	void *buf;
	size_t size;
	off_t addr;
};

void io_worker();
bool issue_io_request(io_work_type type, void *buf = nullptr,
	size_t size = 0, off_t addr = 0, instant_t deadline = instant_t::max());
bool issue_open(io_work_type type, const char *path);
bool io_completed();
bool try_complete_io_request(instant_t deadline);
bool try_complete_io_request(time_interval timeout);
void complete_io_request();

