#include "timing.hpp"


GLsync fence_insert(GLsync old)
{
	glDeleteSync(old);
	return glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool fence_try_wait(GLsync fence, time_interval timeout)
{
	const auto status = glClientWaitSync(fence, 0, timeout.count());
	bool ok = (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED);
	return ok;
}

bool fence_try_wait(GLsync fence, instant_t deadline)
{
	return fence_try_wait(fence, deadline - clk::now());
}

void fence_block(GLsync fence)
{
	glFlush();
	fence_try_wait(fence, time_interval::max());
}

