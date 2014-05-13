#include "opengl_application.h"
#include "gl/util.h"
#include <iostream>

#include "junk_create_icosahedron.h"

class handler {
    GLuint ball_diffuse_tex_id;
    GLuint ball_program_id;
    GLuint ball_vao_id;

public:
    void onContextCreated() {
        std::cout << glGetString(GL_VERSION) << '\n';

        ball_diffuse_tex_id = gl::load_png_texture("textures/ball_albedo.png");
        ball_vao_id = CreateIcosahedron();

        const std::pair<const char*, GLuint> ball_shaders[] {
            { "shaders/vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/fragment.glsl", GL_FRAGMENT_SHADER },
        };

        ball_program_id = gl::load_shader_program(ball_shaders);
        glBindFragDataLocation(ball_program_id, 0, "f_frag_data");

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

    void onRender() noexcept {
        glClearColor(1, 0.5f, 0.25f, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(ball_program_id);
        glBindVertexArray(ball_vao_id);
        glDrawElements(GL_TRIANGLES, 60, GL_UNSIGNED_INT, nullptr);
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
