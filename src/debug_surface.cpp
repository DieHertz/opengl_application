#include "debug_surface.h"
#include "gl/util.h"

debug_surface::~debug_surface() {
    glDeleteBuffers(1, &vbo_id);
    glDeleteVertexArrays(1, &vao_id);
    glDeleteProgram(program_id);
}

void debug_surface::init() {
    const std::pair<const char*, GLenum> shaders[] {
        { "shaders/debug_vertex.glsl", GL_VERTEX_SHADER },
        { "shaders/debug_fragment.glsl", GL_FRAGMENT_SHADER },
    };

    const auto program_id = gl::load_shader_program(shaders);
    gl::link_shader_program(program_id);

    this->program_id = program_id;

    const GLfloat vertices[] {
        -1, 1,
        -1, -1,
        1, -1,
        -1, 1,
        1, -1,
        1, 1
    };

    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(vertices[0]), 0);
}

void debug_surface::draw(const GLuint tex_id, const glm::vec4 rect) const {
    glViewport(rect.x, rect.y, rect.z, rect.w);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    glUseProgram(program_id);
    glUniform1i(glGetUniformLocation(program_id, "u_texture"), 0);

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
