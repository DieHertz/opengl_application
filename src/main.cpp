#include "noexcept.h"
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

    GLuint diffuse_map_loc, mv_loc, mvp_loc, eye_position_loc;

    glm::vec3 eye{0, 0, 3};
    glm::vec3 center;
    glm::vec3 up{0, 1, 0};

    glm::mat4 model, view, projection;
    glm::mat4 mv, mvp;
    glm::mat3 normal;

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
            { diffuse_map_loc, "u_diffuse_map" },
            { mv_loc, "u_mp" },
            { mvp_loc, "u_mvp" },
            { eye_position_loc, "u_eye_position" }
        };

        for (auto& uniform : uniforms) {
            uniform.first = glGetUniformLocation(program_id, uniform.second);
        }
    }

public:
    void onContextCreated() {
        glEnable(GL_DEPTH_TEST);

        ball_diffuse_tex_id = gl::load_png_texture("textures/ball_diffuse.png");

        ball = mesh::gen_sphere(0.5f, 32, 32);

        const std::pair<const char*, GLuint> ball_shaders[] {
            { "shaders/vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/fragment.glsl", GL_FRAGMENT_SHADER },
        };

        ball_program_id = gl::load_shader_program(ball_shaders);
        glBindFragDataLocation(ball_program_id, 0, "f_frag_color");

        try {
            gl::link_shader_program(ball_program_id);
        } catch (...) {
            glDeleteProgram(ball_program_id);
            throw;
        }

        getUniformLocations(ball_program_id);
    }

    void onCursorMove(const double x, const double y) NOEXCEPT {
        if (!lmb_down) return;

        const auto factor = 1.0f;
        const auto diff = factor * (glm::vec2(x, y) - prev_mouse_pos);
        camera_left(diff.x);
        camera_up(-diff.y);

        look_at(eye, center, up);

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onMouseButton(const int button, const int action, const int mods,
        const double x, const double y) NOEXCEPT {
        lmb_down = button == 0 && action == 1;

        if (!lmb_down) return;

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onFramebufferResize(const int width, const int height) NOEXCEPT {
        glViewport(0, 0, width, height);
        look_at(eye, center, up);
        perspective(60, static_cast<float>(width) / height, 0.1f, 5.0f);
    }

    void onRender() NOEXCEPT {
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ball_program_id);
        glUniform1i(diffuse_map_loc, 0);
        glUniformMatrix4fv(mv_loc, 1, GL_FALSE, glm::value_ptr(mv));
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1fv(eye_position_loc, 4, glm::value_ptr(eye));

        glBindTexture(GL_TEXTURE_2D, ball_diffuse_tex_id);

        glBindVertexArray(ball.vao_id);

        glDrawElements(ball.mode, ball.num_faces, ball.type, nullptr);
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
