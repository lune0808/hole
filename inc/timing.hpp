#pragma once

#include <chrono>
#include <glad/gl.h>


using clk = std::chrono::steady_clock;
using instant_t = std::chrono::time_point<clk>;
using time_interval = std::chrono::nanoseconds;

GLsync fence_insert(GLsync old = nullptr);
bool fence_try_wait(GLsync fence, time_interval timeout);
bool fence_try_wait(GLsync fence, instant_t deadline);
void fence_block(GLsync fence);
