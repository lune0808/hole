#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>

struct glfw_context
{
	glfw_context()
	{
		glfwInit();
	}

	~glfw_context()
	{
		glfwTerminate();
	}
};

struct window : public glfw_context
{
	GLFWwindow *handle;

	window(int width, int height)
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		const char *title = "window";
		handle = glfwCreateWindow(width, height, title, NULL, NULL);
		glfwMakeContextCurrent(handle);
		gladLoadGL(glfwGetProcAddress);
		glViewport(0, 0, width, height);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glEnable(GL_DEPTH_TEST);
	}

	~window()
	{
		glfwDestroyWindow(handle);
	}

	operator bool() const
	{
		return !glfwWindowShouldClose(handle);
	}

	void draw()
	{
		glfwSwapBuffers(handle);
		glfwPollEvents();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
};

