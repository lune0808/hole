#pragma once

#include <glad/gl.h>

GLuint compile_shader(const char *src, GLenum kind);
GLuint build_shader(const char *vert_src, const char *frag_src);
GLuint build_shader(const char *comp_src);
