#ifndef gl_util_h
#define gl_util_h

#include "gl_include.h"
#include <vector>
#include <string>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace gl {

template<typename container>
container load_file(const std::string& name) {
    std::basic_ifstream<char> file{name, std::ios::binary};
    if (!file) throw std::runtime_error{"could not open " + name};

    return container(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
}

GLuint load_shader(const char* file_name, GLenum type);

template<size_t N>
GLuint load_shader_program(const std::pair<const char*, GLenum> (&shaders)[N]) {
    const auto program_id = glCreateProgram();
    if (!program_id) throw std::runtime_error{"glCreateProgram() failed"};

    for (auto& shader : shaders) {
        const auto shader_id = load_shader(shader.first, shader.second);
        glAttachShader(program_id, shader_id);
    }

    return program_id;
}

void link_shader_program(GLuint program_id, bool delete_on_fail = true);

GLuint load_png_texture(const char* name);
GLuint load_png_texture_cube(const char* name);

} /* namespace gl */

#endif /* gl_util_h */
