#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "async.hpp"

file::file(const char *path, mode m, size_t count)
	: ctrl{}, buf{std::make_unique<std::uint32_t[]>(count)}
{
#ifndef NDEBUG
	std::memset(buf.get(), 0x66, count * sizeof(buf[0]));
#endif

	int fd;
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
	ctrl.aio_fildes = fd;
	if (fd == -1) return;
}

file::~file()
{
	int fd = ctrl.aio_fildes;
	fsync(fd);
	close(fd);
}

off_t file::read(size_t size, off_t at)
{
	ctrl.aio_offset = at;
	ctrl.aio_buf = buf.get();
	ctrl.aio_nbytes = size;
	ctrl.aio_reqprio = 0;
	ctrl.aio_sigevent.sigev_notify = SIGEV_NONE;
	aio_read(&ctrl);
	return at + size;
}

off_t file::write(size_t size, off_t at)
{
	ctrl.aio_offset = at;
	ctrl.aio_buf = buf.get();
	ctrl.aio_nbytes = size;
	ctrl.aio_reqprio = 0;
	ctrl.aio_sigevent.sigev_notify = SIGEV_NONE;
	aio_write(&ctrl);
	return at + size;
}

bool file::execute(std::chrono::milliseconds timeout_)
{
	int status;

	status = aio_error(&ctrl);
	if (status == 0) {
		return true;
	} else if (status == EINPROGRESS) {
		auto cb_ptr = &ctrl;
		timespec timeout{
			.tv_sec=0,
			.tv_nsec=timeout_.count() * 1'000'000l
		};
		status = aio_suspend(&cb_ptr, 1, timeout_ != wait? &timeout: nullptr);
		if (status == 0) {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

bool file::cancel()
{
	int status = aio_cancel(ctrl.aio_fildes, &ctrl);
	return (status == AIO_CANCELED) || (status == AIO_ALLDONE);
}

