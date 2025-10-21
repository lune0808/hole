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
		fd = open(path, O_WRONLY|O_CREAT|O_TRUNC|O_SYNC, S_IRUSR|S_IWUSR);
		break;
	case mode::append:
		fd = open(path, O_WRONLY|O_CREAT|O_APPEND|O_SYNC, S_IRUSR|S_IWUSR);
		break;
	default:
		fd = -1;
		break;
	}
}

file::~file()
{
	close(fd);
}

static file::request build_req(int fd, void *buf, size_t size, off_t at)
{
	file::request req = {0};
	req.aio_fildes = fd;
	req.aio_buf = buf;
	req.aio_nbytes = size;
	req.aio_offset = at;
	req.aio_sigevent.sigev_notify = SIGEV_NONE;
	return req;
}

file::request file::read(void *buf, size_t size, off_t at)
{
	request req = build_req(fd, buf, size, at);
	req.is_read = true;
	aio_read(&req);
	return req;
}

file::request file::write(void *buf, size_t size, off_t at)
{
	request req = build_req(fd, const_cast<void*>(buf), size, at);
	req.is_read = false;
	aio_write(&req);
	return req;
}

bool file::execute(request req, std::chrono::milliseconds timeout_)
{
	off_t status;

	status = aio_error(&req);
	assert(fd == req.fd());
	if (status == 0) {
		status = aio_return(&req);
		assert(status == req.size());
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
			if (status != req.size()) {
				if (status < 0) {
					std::printf("aio error %m\n");
					return false;
				}
				off_t addr = req.addr() + status;
				auto size = req.size() - status;
				char *buf = static_cast<char*>(req.buf()) + status;
				status = ::lseek(req.fd(), addr, SEEK_SET);
				if (status != addr) {
					std::printf("lseek %m\n");
					return false;
				}
				int time_stuck = 0;
				do {
					errno = 0;
					status = req.is_read? ::read(req.fd(), buf, size): ::write(req.fd(), buf, size);
					std::printf("blocking %s (%zx)!!!\n", req.is_read? "read": "write", status);
					if (status > 0) {
						size -= status;
						buf += status;
						time_stuck = 0;
					} else if (status == 0) {
						if (++time_stuck == 8) {
							goto blocking;
						}
					} else {
					blocking:
						std::printf("error forcing blocking %s!!!!!!!!! %m\n", req.is_read? "read": "write");
						return false;
					}
				} while (size != 0);
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

