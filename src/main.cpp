#include "noexcept.h"
#include "opengl_application.h"
#include "mesh/mesh.h"
#include "gl/util.h"
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

struct scene_object {
    mesh::mesh_data mesh;
    glm::vec4 diffuse_color;
    GLuint diffuse_tex_id;
    GLuint program_id;
};

struct light {
    glm::vec4 pos;
    glm::vec4 color;
};

class handler {
    scene_object ball;
    scene_object plane;
    std::vector<light> lights;

    glm::vec3 eye{-1, 1.5, 3};
    glm::vec3 center;
    glm::vec3 up{0, 1, 0};

    glm::mat4 model, view, projection;
    glm::mat4 mv, mvp;
    glm::mat3 normal_matrix;

    bool lmb_down = false;
    glm::vec2 prev_mouse_pos;

    void look_at(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        view = glm::lookAt(eye, center, up);
        mv = view * model;
        normal_matrix = glm::transpose(glm::inverse(glm::mat3(mv)));
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

    void bindUniforms(const GLuint program_id) {
        glUniform1i(glGetUniformLocation(program_id, "u_diffuse_map"), 0);
        glUniformMatrix4fv(glGetUniformLocation(program_id, "u_mv"), 1, GL_FALSE, glm::value_ptr(mv));
        glUniformMatrix4fv(glGetUniformLocation(program_id, "u_mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix3fv(glGetUniformLocation(program_id, "u_normal_matrix"), 1, GL_FALSE, glm::value_ptr(normal_matrix));
        glUniform1fv(glGetUniformLocation(program_id, "u_eye_position"), 4, glm::value_ptr(eye));
    }

    scene_object create_ball() {
        const auto mesh = mesh::gen_sphere(0.5f, 32, 32);
        const auto diffuse_tex_id = gl::load_png_texture("textures/ball12_diffuse.png");

        const std::pair<const char*, GLuint> shaders[] {
            { "shaders/lighting_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/lighting_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        return { mesh, { 0, 0, 0, 1 }, diffuse_tex_id, program_id };
    }

    scene_object create_plane() {
        const auto mesh = mesh::gen_quad(
            glm::vec3(-3, -0.5, -3), glm::vec3(-3, -0.5, 3),
            glm::vec3(3, -0.5, 3), glm::vec3(3, -0.5, -3));

        const std::pair<const char*, GLuint> shaders[] {
            { "shaders/lighting_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/lighting_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        return { mesh, { 0.039f, 0.424f, 0.012f, 1 }, 0, program_id };
    }

public:
    void onContextCreated() {
        glEnable(GL_DEPTH_TEST);

        ball = create_ball();
        plane = create_plane();
        lights = {
            { { 5, 5, 5, 1 }, { 1, 1, 1, 1 } },
            { { -5, -4, 3, 1 }, { 1, 0.5, 1, 1 } }
        };
    }

    void onCursorMove(const double x, const double y) NOEXCEPT {
        if (!lmb_down) return;

        const auto factor = 1.0f;
        const auto diff = factor * (glm::vec2(x, y) - prev_mouse_pos);
        camera_left(diff.x);
        // camera_up(-diff.y);

        look_at(eye, center, up);

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onScroll(const float dx, const float dy) NOEXCEPT {
        const auto eye = this->eye * (1 - 0.1f * dy);
        const auto eye_dist = glm::length(eye);
        if (!(1 < eye_dist && eye_dist < 5)) return;

        this->eye = eye;
        look_at(eye, center, up);
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
        perspective(60, static_cast<float>(width) / height, 0.1f, 10.0f);
    }

    void onRender() NOEXCEPT {
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ball.program_id);
        bindUniforms(ball.program_id);
        glBindTexture(GL_TEXTURE_2D, ball.diffuse_tex_id);
        glUniform1i(glGetUniformLocation(ball.program_id, "u_textured"), true);
        glBindVertexArray(ball.mesh.vao_id);
        glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

        glUseProgram(plane.program_id);
        bindUniforms(plane.program_id);
        glUniform1i(glGetUniformLocation(plane.program_id, "u_textured"), false);
        glUniform4fv(glGetUniformLocation(plane.program_id, "u_diffuse"), 1, glm::value_ptr(plane.diffuse_color));
        glBindVertexArray(plane.mesh.vao_id);
        glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);
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
