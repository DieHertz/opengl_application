#include "opengl_application.h"
#include "mesh/mesh.h"
#include "gl/util.h"
#include <iostream>

class handler {
    mesh::mesh_data ball;
    GLuint ball_diffuse_tex_id;
    GLuint ball_program_id;

    GLuint sampler_loc;

public:
    void onContextCreated() {
        ball_diffuse_tex_id = gl::load_png_texture("textures/ball_albedo.png");

        ball = mesh::gen_sphere(0.5f, 8);

        const std::pair<const char*, GLuint> ball_shaders[] {
            { "shaders/vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/fragment.glsl", GL_FRAGMENT_SHADER },
        };

        ball_program_id = gl::load_shader_program(ball_shaders);
        glBindFragDataLocation(ball_program_id, 0, "f_frag_data");
        sampler_loc = glGetUniformLocation(ball_program_id, "s_tex");

        try {
            gl::link_shader_program(ball_program_id);
        } catch (...) {
            glDeleteProgram(ball_program_id);
            throw;
        }
    }

    void onCursorMove(const double x, const double y) noexcept {

    }

    void onMouseButton(const int button, const int action, const int mods) noexcept {

    }

    void onFramebufferResize(const int width, const int height) noexcept {
        glViewport(0, 0, width, height);
    }

    void onRender() noexcept {
        glClearColor(1, 0.5f, 0.25f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ball_program_id);
        glUniform1i(sampler_loc, 0);
        glBindTexture(GL_TEXTURE_2D, ball_diffuse_tex_id);
        glBindVertexArray(ball.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, ball.num_vertices);
    }
};

int main() {
    try {
        auto app = opengl_application<handler>{4, 1, 640, 480};
        app.run();
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
}
