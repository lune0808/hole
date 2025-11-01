#pragma once

#include "std.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>

struct glfw_context
{
	glfw_context();
	~glfw_context();
};

struct window
{
	GLFWwindow *handle;

	window(int width, int height, glfw_context &);
	~window();

	operator bool() const;
	void resize(int width, int height);
	void present();
};

