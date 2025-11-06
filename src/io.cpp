#include "std.hpp"
#include "libs.hpp"
#include "io.hpp"


using namespace std::chrono_literals;
static constexpr size_t io_concurrency = 1u;
static io_uring uring;
static int fd;

void io_init()
{
	int status = io_uring_queue_init(io_concurrency, &uring, 0);
	if (status < 0) {
		std::fprintf(stderr, "failed to setup uring\n");
		std::exit(1);
	}
	fd = 0;
}

void io_fini()
{
	io_uring_queue_exit(&uring);
}

bool issue_io_request(io_work_type type, void *buf, size_t size, off_t addr)
{
	assert(size < std::numeric_limits<int>::max());
	auto sqe = io_uring_get_sqe(&uring);
	int status = 0;
	switch (type) {
	case io_work_type::read:
		io_uring_prep_read(sqe, fd, buf, size, addr);
		sqe->user_data = size;
		status = io_uring_submit(&uring);
		break;
	case io_work_type::write:
		io_uring_prep_write(sqe, fd, buf, size, addr);
		sqe->user_data = size;
		status = io_uring_submit(&uring);
		break;
	}
	if (status < 0) {
		std::fprintf(stderr, "[I/O error] %m\n");
	}
	return status >= 0;
}

void blocking_open_read(const char *path)
{
	assert(!fd);
	fd = open(path, O_RDONLY);
	assert(fd > 0);
}

void blocking_open_trunc(const char *path)
{
	assert(!fd);
	fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	assert(fd > 0);
}

void blocking_open_recover(const char *path)
{
	assert(!fd);
	fd = open(path, O_RDWR);
	assert(fd > 0);
}

void blocking_close()
{
	assert(fd > 0);
	close(fd);
#ifndef NDEBUG
	fd = 0;
#endif
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

bool try_complete_io_request(time_interval timeout)
{
	io_uring_cqe *cqe;
	auto ts = duration2timespec(timeout);
	int status = io_uring_wait_cqe_timeout(&uring, &cqe, &ts);
	int expected = 0;
	if (status == 0) {
		status = cqe->res;
		expected = (int) cqe->user_data;
		io_uring_cqe_seen(&uring, cqe);
		assert(status == expected);
	}
	return status == expected;
}

bool try_complete_io_request(instant_t deadline)
{
	return try_complete_io_request(deadline - clk::now());
}

void complete_io_request()
{
	try_complete_io_request(instant_t::max());
}

