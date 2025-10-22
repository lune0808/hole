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

static inline void APIENTRY dbg_callback(
	GLenum source, GLenum type, GLuint, GLenum severity,
	GLsizei len, const GLchar *msg, const void *
)
{
	const char *ssource;
	switch (source) {
	case GL_DEBUG_SOURCE_API:
		ssource = "API";
		break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		ssource = "WS";
		break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		ssource = "SC";
		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:
		ssource = "3P";
		break;
	case GL_DEBUG_SOURCE_APPLICATION:
		ssource = "APP";
		break;
	default:
		ssource = "UNK";
		break;
	}

	const char *stype;
	switch (type) {
	case GL_DEBUG_TYPE_ERROR:
		stype = "ERR";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		stype = "DEPR";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		stype = "UB";
		break;
	case GL_DEBUG_TYPE_PORTABILITY:
		stype = "PORT";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE:
		stype = "PERF";
		break;
	default:
		stype = "UNK";
		break;
	}

	const char *sseverity;
	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:
		sseverity = "high";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		sseverity = "medium";
		break;
	case GL_DEBUG_SEVERITY_LOW:
		sseverity = "low";
		break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		sseverity = "note";
		break;
	default:
		sseverity = "unknown";
		break;
	}

	std::fprintf(stderr, "[GL %s(%s)]: %*s\n",
		stype, sseverity, static_cast<int>(len), msg);
}

struct window
{
	GLFWwindow *handle;

	window(int width, int height, glfw_context &)
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		const char *title = "window";
		handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
		glfwMakeContextCurrent(handle);
		gladLoadGL(glfwGetProcAddress);
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(dbg_callback, nullptr);
		glViewport(0, 0, width, height);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
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
		glClear(GL_COLOR_BUFFER_BIT);
	}
};

