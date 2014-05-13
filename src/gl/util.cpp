#include "util.h"
#include "picopng.h"

namespace gl {

GLuint load_shader(const char* file_name, const GLuint type) {
    const auto shader_src = load_file<std::string>(file_name);
    if (shader_src.empty()) throw std::runtime_error{std::string{file_name} + " - shader is empty"};

    const auto shader_id = glCreateShader(type);
    if (!shader_id) throw std::runtime_error{"glCreateShader() failed"};

    const GLchar* src_array[] { shader_src.data() };
    glShaderSource(shader_id, 1, src_array, nullptr);
    glCompileShader(shader_id);

    GLint compiled;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint info_len = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_len);

        auto info = std::string(info_len, char{});
        glGetShaderInfoLog(shader_id, info_len, nullptr, &info[0]);

        glDeleteShader(shader_id);

        throw std::runtime_error{std::string{"glCompileShader(\""} + file_name + "\") failed:\n" + info};
    }

    return shader_id;
}

void link_shader_program(const GLuint program_id) {
    glLinkProgram(program_id);

    GLint linked;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint info_len = 0;

        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_len);

        auto info = std::string(info_len, char{});
        glGetProgramInfoLog(program_id, info_len, nullptr, &info[0]);

        throw std::runtime_error{"glLinkProgram() failed:\n" + info};
    }
}

GLuint load_png_texture(const char* name) {
    const auto png_bytes = load_file<std::vector<uint8_t>>(name);

    auto image_bytes = std::vector<uint8_t>{};
    unsigned long width, height;
    const auto ret_code = decodePNG(image_bytes, width, height, png_bytes.data(), png_bytes.size());
    if (ret_code) throw std::runtime_error{"decodePNG() failed"};

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_bytes.data());

    return tex_id;
};

} /* namespace gl */
