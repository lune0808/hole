#pragma once

#include <glad/gl.h>
#include <span>

GLuint compile_shader(const char *src, GLenum kind);
GLuint build_shader(const char *vert_src, const char *frag_src);
GLuint build_shader(const char *comp_src);
size_t file_size(const char *path);
void load_file(const char *path, char *buf, size_t size, char terminator);
void load_file(const char *path, std::span<char> buf, char terminator);
