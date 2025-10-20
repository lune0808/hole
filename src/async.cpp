#include <fcntl.h>
#include <unistd.h>
#include "async.hpp"

file::file(const char *path, mode_t mode)
	: ctrl{}, pos{0}
{
	int fd;
	switch (mode) {
	case mode_t::read:
		fd = open(path, O_RDONLY);
		break;
	case mode_t::write:
		fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
		break;
	case mode_t::append:
		fd = open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
		break;
	default:
		fd = -1;
		break;
	}
	ctrl.aio_fildes = fd;
	if (fd == -1) return;
}

file::~file()
{
	close(ctrl.aio_fildes);
}

void file::read(void *buf, size_t size)
{
	ctrl.aio_offset = pos;
	ctrl.aio_buf = buf;
	ctrl.aio_nbytes = size;
	ctrl.aio_reqprio = 0;
	ctrl.aio_sigevent.sigev_notify = SIGEV_NONE;
	aio_read(&ctrl);
}

void file::write(const void *buf, size_t size)
{
	ctrl.aio_offset = pos;
	ctrl.aio_buf = const_cast<void*>(buf);
	ctrl.aio_nbytes = size;
	ctrl.aio_reqprio = 0;
	ctrl.aio_sigevent.sigev_notify = SIGEV_NONE;
	aio_write(&ctrl);
}

bool file::execute(std::chrono::milliseconds timeout_)
{
	int io_state = aio_error(&ctrl);
	if (io_state == 0) {
		pos += aio_return(&ctrl);
		return true;
	} else if (io_state == EINPROGRESS) {
		auto cb_ptr = &ctrl;
		timespec timeout{
			.tv_sec=0,
			.tv_nsec=timeout_.count() * 1'000'000l
		};
		int status = aio_suspend(&cb_ptr, 1, &timeout);
		if (status == 0) {
			pos += aio_return(&ctrl);
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

