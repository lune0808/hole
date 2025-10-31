#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include "liburing.h"
#include "io.hpp"


using namespace std::chrono_literals;
static constexpr auto poll_period = 1ms;
static constexpr size_t io_concurrency = 1u;
static io_uring uring;
static int fd;

void io_worker()
{
	int status = io_uring_queue_init(io_concurrency, &uring, 0);
	if (status < 0) {
		std::fprintf(stderr, "failed to setup uring\n");
		std::exit(1);
	}
	fd = 0;
}

bool issue_io_request(io_work_type type, void *buf,
	size_t size, off_t addr, instant_t deadline)
{
	int status = 0;
	const char *path = (const char*) buf;
	switch (type) {
	case io_work_type::none:
		break;
	case io_work_type::open_read:
		assert(!fd);
		status = open(path, O_RDONLY);
		fd = status;
		break;
	case io_work_type::open_write:
		assert(!fd);
		status = open(path, O_WRONLY|O_TRUNC|O_CREAT);
		fd = status;
		break;
	case io_work_type::open_recover:
		assert(!fd);
		status = open(path, O_RDWR);
		fd = status;
		break;
	case io_work_type::close:
		assert(fd > 0);
		close(fd);
#ifndef NDEBUG
		fd = 0;
#endif
		break;
	case io_work_type::read:
		io_uring_prep_read(io_uring_get_sqe(&uring), fd, buf, size, addr);
		status = io_uring_submit(&uring);
		break;
	case io_work_type::write:
		io_uring_prep_write(io_uring_get_sqe(&uring), fd, buf, size, addr);
		status = io_uring_submit(&uring);
		break;
	case io_work_type::exit:
		io_uring_queue_exit(&uring);
		break;
	}
	if (status < 0) {
		std::fprintf(stderr, "[I/O error] %m\n");
	}
	(void) deadline;
	return status >= 0;
}

bool issue_open(io_work_type type, const char *path)
{
	assert(type == io_work_type::open_read   ||
	       type == io_work_type::open_write  ||
	       type == io_work_type::open_recover);
	return issue_io_request(type, const_cast<char*>(path));
}

bool io_completed(io_uring_cqe **pcqe)
{
	return io_uring_peek_cqe(&uring, pcqe) == 0;
}

static __kernel_timespec duration2timespec(time_interval ti)
{
	__kernel_timespec ts;
	ts.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(ti).count();
	ts.tv_nsec = std::chrono::nanoseconds(ti).count() - 1'000'000'000ULL * ts.tv_sec;
	return ts;
}

bool try_complete_io_request(io_work_type type, time_interval timeout)
{
	if (type != io_work_type::read && type != io_work_type::write) {
		return true;
	}
	io_uring_cqe *cqe;
	auto ts = duration2timespec(timeout);
	int status = io_uring_wait_cqe_timeout(&uring, &cqe, &ts);
	auto status2 = cqe->res;
	io_uring_cqe_seen(&uring, cqe);
	return status == 0 && status2 > 0;
}

bool try_complete_io_request(io_work_type type, instant_t deadline)
{
	return try_complete_io_request(type, deadline - clk::now());
}

void complete_io_request(io_work_type type)
{
	try_complete_io_request(type, instant_t::max());
}

