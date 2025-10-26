#include <fstream>
#include <thread>
#include <cstring>
#include <cassert>
#include "io.hpp"


using namespace std::chrono_literals;
static constexpr auto poll_period = 1ms;
static std::atomic<io_work_type> current_io_work_type;
static io_request current_io_request;

void io_worker()
{
	const auto type = &current_io_work_type;
	const auto preq = &current_io_request;
	std::fstream s;
	for (;;) {
		const auto t = type->load(std::memory_order_acquire);
		const auto req = *preq;
		const auto buf = reinterpret_cast<char*>(req.buf);
		switch (t) {
		case io_work_type::none:
			std::this_thread::sleep_for(poll_period);
			continue;
		case io_work_type::open_write:
			s = std::fstream(buf, std::ios::out | std::ios::binary);
			break;
		case io_work_type::open_read:
			s = std::fstream(buf, std::ios::in  | std::ios::binary);
			break;
		case io_work_type::open_recover:
			s = std::fstream(buf, std::ios::in  | std::ios::out | std::ios::binary);
			break;
		case io_work_type::close:
			s.close();
			break;
		case io_work_type::write:
			assert(s.is_open());
			if (!(s.seekp(req.addr) && s.write(buf, req.size))) {
				std::printf("[io error] write\n");
				std::memset(buf, 'W' /* 0x57 */, req.size);
			}
			break;
		case io_work_type::read:
			assert(s.is_open());
			if (!(s.seekg(req.addr) && s.read(buf, req.size))) {
				std::printf("[io error] read\n");
				std::memset(buf, 'R' /* 0x52 */, req.size);
			}
			break;
		case io_work_type::exit:
			std::printf("closing streams...\r");
			return;
		}
		type->store(io_work_type::none, std::memory_order_release);
	}
}

bool issue_io_request(io_work_type type, void *buf,
	size_t size, off_t addr, instant_t deadline)
{
	// since we only have 1 producer thread, this is fine
	while (current_io_work_type.load(std::memory_order_relaxed) != io_work_type::none) {
		if (clk::now() >= deadline) {
			return false;
		}
		std::this_thread::sleep_for(poll_period);
	}
	current_io_request = io_request{buf, size, addr};
	current_io_work_type.store(type, std::memory_order_release);
	return true;
}

bool issue_open(io_work_type type, const char *path)
{
	assert(type == io_work_type::open_read   ||
	       type == io_work_type::open_write  ||
	       type == io_work_type::open_recover);
	return issue_io_request(type, const_cast<char*>(path));
}

bool io_completed()
{
	// we need acquire semantics to see the io operation's result in memory
	return current_io_work_type.load(std::memory_order_acquire) == io_work_type::none;
}

bool try_complete_io_request(instant_t deadline)
{
	for (;;) {
		const auto now = clk::now();
		if (now >= deadline) {
			return false;
		} else if (io_completed()) {
			return true;
		}
		std::this_thread::sleep_for(poll_period);
	}
}

bool try_complete_io_request(time_interval timeout)
{
	return try_complete_io_request(clk::now() + timeout);
}

void complete_io_request()
{
	try_complete_io_request(instant_t::max());
}
