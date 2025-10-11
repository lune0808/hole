#pragma once

#include <cstdio>
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
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback([] APIENTRY
				(GLenum source, GLenum type, GLuint, GLenum severity, GLsizei len, const GLchar *msg, const void*)
		{
			std::fprintf(stderr, "[GL %s] type 0x%x severity 0x%x : %*s\n",
					(type == GL_DEBUG_TYPE_ERROR)? "error": "warning", type, severity, static_cast<int>(len), msg);
		}, nullptr);
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

