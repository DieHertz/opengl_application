#include "util.h"
#include "picopng.h"
#include <cstdint>

namespace gl {

GLuint load_shader(const char* file_name, const GLenum type) {
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

void link_shader_program(const GLuint program_id, const bool delete_on_fail) {
    glLinkProgram(program_id);

    GLint linked;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint info_len = 0;

        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_len);

        auto info = std::string(info_len, char{});
        glGetProgramInfoLog(program_id, info_len, nullptr, &info[0]);

        if (delete_on_fail) glDeleteProgram(program_id);

        throw std::runtime_error{"glLinkProgram() failed:\n" + info};
    }
}

std::vector<uint8_t> load_png_bytes(const std::string& name, unsigned long& width, unsigned long& height) {
    const auto png_bytes = load_file<std::vector<uint8_t>>(name);

    auto image_bytes = std::vector<uint8_t>{};
    const auto ret_code = decodePNG(image_bytes, width, height, png_bytes.data(), png_bytes.size());
    if (ret_code) throw std::runtime_error{"decodePNG() failed"};

    return image_bytes;
}

GLuint load_png_texture(const char* name) {
    unsigned long width, height;
    const auto bytes = load_png_bytes(name, width, height);

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());

    return tex_id;
};

GLuint load_png_texture_cube(const char* name) {
    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    const char* face_names[] { "posx", "negx", "posy", "negy", "posz", "negz" };

    for (auto face = 0; face < 6; ++face) {
        unsigned long width, height;
        const auto bytes = load_png_bytes(std::string{name} + '/' + face_names[face] + ".png", width, height);
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA,
            width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data()
        );
    }

    return tex_id;
}

} /* namespace gl */
