#pragma once

#include <chrono>
#include "timing.hpp"


enum class io_work_type {
	read,
	write,
};

struct io_request
{
	void *buf;
	size_t size;
	off_t addr;
};

void io_init();
void io_fini();
bool issue_io_request(io_work_type type, void *buf, size_t size, off_t addr);
void blocking_open_read(const char *path);
void blocking_open_trunc(const char *path);
void blocking_open_recover(const char *path);
void blocking_close();
bool try_complete_io_request(instant_t deadline);
bool try_complete_io_request(time_interval timeout);
void complete_io_request();

