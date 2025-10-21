#include <fcntl.h>
#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <cstring>
#include "async.hpp"

file::file(const char *path, mode m, size_t count)
{
	switch (m) {
	case mode::read:
		fd = open(path, O_RDONLY);
		break;
	case mode::write:
		fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
		break;
	case mode::append:
		fd = open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
		break;
	default:
		fd = -1;
		break;
	}
}

file::~file()
{
	fsync(fd);
	close(fd);
}

static file::request build_req(int fd, void *buf, size_t size, off_t at)
{
	file::request req;
	req.aio_fildes = fd;
	req.aio_buf = buf;
	req.aio_nbytes = size;
	req.aio_offset = at;
	req.aio_reqprio = 0;
	req.aio_sigevent.sigev_notify = SIGEV_NONE;
	return req;
}

file::request file::read(void *buf, size_t size, off_t at)
{
	request req = build_req(fd, buf, size, at);
	aio_read(&req);
	return req;
}

file::request file::write(void *buf, size_t size, off_t at)
{
	request req = build_req(fd, const_cast<void*>(buf), size, at);
	aio_write(&req);
	return req;
}

bool file::execute(request req, std::chrono::milliseconds timeout_)
{
	int status;

	status = aio_error(&req);
	assert(req.aio_fildes == fd);
	if (status == 0) {
		status = aio_return(&req);
		assert(status == req.aio_nbytes);
		return true;
	} else if (status == EINPROGRESS) {
		aiocb *cb_ptr = &req;
		timespec timeout{
			.tv_sec=0,
			.tv_nsec=timeout_.count() * 1'000'000l
		};
		status = aio_suspend(&cb_ptr, 1, timeout_ != wait? &timeout: nullptr);
		if (status == 0) {
			status = aio_return(&req);
			if (status != req.aio_nbytes) {
				// assumes writes never reach this point...
				std::printf("blocking read!!!\n");
				::lseek(req.aio_fildes, req.aio_offset, SEEK_SET);
				::read(req.aio_fildes, (void*) req.aio_buf, req.aio_nbytes);
			}
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

bool file::cancel(request req)
{
	int status = aio_cancel(req.aio_fildes, &req);
	return (status == AIO_CANCELED) || (status == AIO_ALLDONE);
}

