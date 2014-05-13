#include "opengl_application.h"
#include "mesh/mesh.h"
#include "gl/util.h"
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

class handler {
    mesh::mesh_data ball;
    GLuint ball_diffuse_tex_id;
    GLuint ball_program_id;

    GLuint sampler_loc, mv_loc, mvp_loc;

    glm::vec3 eye{0, 0, 2};
    glm::vec3 center;
    glm::vec3 up{0, 1, 0};

    glm::mat4 model, view, projection;
    glm::mat4 mv, mvp;

    bool lmb_down = false;
    glm::vec2 prev_mouse_pos;

    void look_at(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        view = glm::lookAt(eye, center, up);
        mv = view * model;
        mvp = projection * mv;
    }

    void perspective(const float fovy, const float aspect, const float zNear, const float zFar) {
        projection = glm::perspective(glm::radians(fovy), aspect, zNear, zFar);
        mvp = projection * mv;
    }

    void camera_up(const float degrees) {
        const auto ortho = glm::normalize(glm::cross(eye, up));
        const auto eye = glm::vec4(this->eye, 1.0f);
        const auto up = glm::vec4(this->up, 1.0f);

        this->eye = glm::vec3(eye * glm::rotate(glm::mat4(), glm::radians(degrees), ortho));
        this->up = glm::vec3(up * glm::rotate(glm::mat4(), glm::radians(degrees), ortho));
    }

    void camera_left(const float degrees) {
        const auto eye = glm::vec4(this->eye, 1.0f) * glm::rotate(glm::mat4(), glm::radians(degrees), up);

        this->eye = glm::vec3(eye);
    }

    void getUniformLocations(const GLuint program_id) {
        const std::pair<GLuint&, const char*> uniforms[] {
            { sampler_loc, "u_tex" },
            { mv_loc, "u_mp" },
            { mvp_loc, "u_mvp" },
        };

        for (auto& uniform : uniforms) {
            uniform.first = glGetUniformLocation(program_id, uniform.second);
        }
    }

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

        try {
            gl::link_shader_program(ball_program_id);
        } catch (...) {
            glDeleteProgram(ball_program_id);
            throw;
        }

        getUniformLocations(ball_program_id);
    }

    void onCursorMove(const double x, const double y) noexcept {
        if (!lmb_down) return;

        const auto factor = 1.0f;
        const auto diff = factor * (glm::vec2(x, y) - prev_mouse_pos);
        camera_left(diff.x);
        camera_up(-diff.y);

        look_at(eye, center, up);

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onMouseButton(const int button, const int action, const int mods,
        const double x, const double y) noexcept {
        lmb_down = button == 0 && action == 1;

        if (!lmb_down) return;

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onFramebufferResize(const int width, const int height) noexcept {
        glViewport(0, 0, width, height);
        look_at(eye, center, up);
        perspective(60, static_cast<float>(width) / height, 0.1f, 5.0f);
    }

    void onRender() noexcept {
        glClearColor(1, 0.5f, 0.25f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ball_program_id);
        glUniform1i(sampler_loc, 0);
        glUniformMatrix4fv(mv_loc, 1, GL_FALSE, glm::value_ptr(mv));
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

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
