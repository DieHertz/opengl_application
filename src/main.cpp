#include "noexcept.h"
#include "opengl_application.h"
#include "mesh/mesh.h"
#include "gl/util.h"
#include "ext.h"
#include "scope_exit.h"
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

const auto MAX_LIGHTS = 8;
const auto FIRST_SHADOW_MAP_TIU = GL_TEXTURE3;

struct material {
    glm::vec4 diffuse;
    glm::vec4 specular;
    float shininess;
};

struct scene_object {
    mesh::mesh_data mesh;
    material mtl;
    GLuint mtl_buffer_id;
    GLuint diffuse_tex_id;
    GLuint normal_tex_id;
};

struct light {
    glm::vec4 pos;
    glm::vec4 color;
};

struct transformations {
    glm::mat4 depth_bias_matrix;
    glm::mat4 mv_matrix;
    glm::mat4 mvp_matrix;
    glm::mat4 normal_matrix;
};

class handler {
    scene_object ball;
    scene_object plane;

    std::vector<light> lights;
    std::vector<glm::mat4> shadow_map_mvp_matrices;
    std::vector<GLuint> shadow_map_tex_ids;

    glm::vec3 eye{-1, 1.5f, 3};
    glm::vec3 center{0, 0, 0};
    glm::vec3 up{0, 1, 0};

    const glm::mat4 depth_bias_matrix{
        0.5f,   0,      0,      0,
        0,      0.5f,   0,      0,
        0,      0,      0.5f,   0,
        0.5f,   0.5f,   0.5f,   1
    };

    glm::mat4 model, view, projection;
    transformations transf;

    GLuint transf_buffer_id;
    const GLuint transf_binding_point = 1;

    GLuint lights_buffer_id;
    const GLuint lights_binding_point = 2;

    const GLuint mtl_binding_point = 3;

    GLuint depth_fbo_id;
    GLuint depth_tex_id;
    GLuint depth_program_id;

    GLuint lighting_program_id;

    bool lmb_down = false;
    glm::vec2 prev_mouse_pos;

    glm::vec2 framebuffer_size;

    void look_at(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        view = glm::lookAt(eye, center, up);
        transf.mv_matrix = view * model;
        transf.normal_matrix = glm::transpose(glm::inverse(transf.mv_matrix));
        transf.mvp_matrix = projection * transf.mv_matrix;
    }

    void perspective(const float fovy, const float aspect, const float zNear, const float zFar) {
        projection = glm::perspective(glm::radians(fovy), aspect, zNear, zFar);
        transf.mvp_matrix = projection * transf.mv_matrix;
    }

    void camera_up(const float degrees) {
        const auto ortho = glm::normalize(glm::cross(eye, up));

        eye = glm::vec3(glm::vec4(eye, 1) * glm::rotate(glm::mat4(), glm::radians(degrees), ortho));
        // up = glm::vec3(glm::vec4(up, 0) * glm::rotate(glm::mat4(), glm::radians(degrees), ortho));
    }

    void camera_left(const float degrees) {
        const auto eye = glm::vec4(this->eye, 1) * glm::rotate(glm::mat4(), glm::radians(degrees), up);

        this->eye = glm::vec3(eye);
    }

    void create_transf_ubo() {
        glGenBuffers(1, &transf_buffer_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, transf_binding_point, transf_buffer_id);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(transformations), nullptr, GL_DYNAMIC_DRAW);
    }

    void update_transf_ubo() {
        glBindBuffer(GL_UNIFORM_BUFFER, transf_buffer_id);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(transformations), &transf);
    }

    void create_lights_ubo() {
        glGenBuffers(1, &lights_buffer_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, lights_binding_point, lights_buffer_id);

        const auto num_lights_offset = MAX_LIGHTS * sizeof(lights.front());
        const auto num_lights = GLint(lights.size());
        glBufferData(GL_UNIFORM_BUFFER, sizeof(num_lights) + num_lights_offset, nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, lights.size() * sizeof(lights.front()), lights.data());
        glBufferSubData(GL_UNIFORM_BUFFER, num_lights_offset, sizeof(num_lights), &num_lights);
    }

    void create_shadow_maps() {
        shadow_map_tex_ids = std::vector<GLuint>(lights.size());
        glGenTextures(lights.size(), &shadow_map_tex_ids[0]);

        scope_exit({ glActiveTexture(GL_TEXTURE0); });

        for (size_t i = 0; i < lights.size(); ++i) {
            glActiveTexture(FIRST_SHADOW_MAP_TIU + i);
            glBindTexture(GL_TEXTURE_2D, shadow_map_tex_ids[i]);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, framebuffer_size.x, framebuffer_size.y,
                0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }

        const auto aspect_ratio = framebuffer_size.x / framebuffer_size.y;
        std::transform(std::begin(lights), std::end(lights), std::back_inserter(shadow_map_mvp_matrices),
            [=] (const light& l) {
                return glm::perspective(glm::radians(120.0f), aspect_ratio, 0.1f, 100.0f) *
                    glm::lookAt(glm::vec3(l.pos), center, up);
            }
        );
    }

    void create_depth_fbo() {
        glActiveTexture(GL_TEXTURE1);
        scope_exit({ glActiveTexture(GL_TEXTURE0); });

        glGenTextures(1, &depth_tex_id);
        glBindTexture(GL_TEXTURE_2D, depth_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, framebuffer_size.x, framebuffer_size.y,
            0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glGenFramebuffers(1, &depth_fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_tex_id, 0);

        glDrawBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error{"create_depth_fbo() failed"};
        }
    }

    GLuint create_depth_shader() {
        const std::pair<const char*, GLenum> shaders[] {
            { "shaders/depth_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/depth_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        const auto transf_block_index = glGetUniformBlockIndex(program_id, "transformations");
        glUniformBlockBinding(program_id, transf_block_index, transf_binding_point);

        return program_id;
    }

    GLuint create_lighting_shader() {
        const std::pair<const char*, GLenum> shaders[] {
            { "shaders/lighting_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/lighting_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        const auto transf_block_index = glGetUniformBlockIndex(program_id, "transformations");
        glUniformBlockBinding(program_id, transf_block_index, transf_binding_point);

        const auto lights_block_index = glGetUniformBlockIndex(program_id, "light_params");
        glUniformBlockBinding(program_id, lights_block_index, lights_binding_point);

        const auto mtl_block_index = glGetUniformBlockIndex(program_id, "material");
        glUniformBlockBinding(program_id, mtl_block_index, mtl_binding_point);

        glUseProgram(program_id);
        scope_exit({ glUseProgram(0); });

        for (auto i = 0; i < MAX_LIGHTS; ++i) {
            const auto uname = "u_shadow_maps[" + std::to_string(i) + ']';
            const auto uloc = glGetUniformLocation(program_id, uname.data());

            glUniform1i(uloc, FIRST_SHADOW_MAP_TIU - GL_TEXTURE0 + i);
        }

        return program_id;
    }

    scene_object create_ball() {
        const auto mesh = mesh::gen_sphere(0.5f, 32, 32);

        const auto diffuse_tex_id = gl::load_png_texture("textures/ball12_diffuse.png");

        const auto mtl = material{ { 0, 0, 0, 1 }, { 1, 1, 1, 1 }, 100 };

        auto mtl_buffer_id = GLuint{};
        glGenBuffers(1, &mtl_buffer_id);
        glBindBuffer(GL_UNIFORM_BUFFER, mtl_buffer_id);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(mtl), &mtl, GL_DYNAMIC_DRAW);

        return { mesh, mtl, mtl_buffer_id, diffuse_tex_id };
    }

    scene_object create_plane() {
        const auto mesh = mesh::gen_quad(
            glm::vec3(-5, -0.5, 5), glm::vec3(5, -0.5, 5),
            glm::vec3(5, -0.5, -5), glm::vec3(-5, -0.5, -5));

        const auto diffuse_tex_id = gl::load_png_texture("textures/table_cloth_diffuse.png");
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        const auto normal_tex_id = gl::load_png_texture("textures/table_cloth_normal.png");
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        const auto mtl = material{ { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, 0 };

        auto mtl_buffer_id = GLuint{};
        glGenBuffers(1, &mtl_buffer_id);
        glBindBuffer(GL_UNIFORM_BUFFER, mtl_buffer_id);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(mtl), &mtl, GL_DYNAMIC_DRAW);

        return { mesh, mtl, mtl_buffer_id, diffuse_tex_id, normal_tex_id };
    }

    void update_shadow_bias_matrices() {
        glUseProgram(lighting_program_id);

        for (size_t i = 0; i < lights.size(); ++i) {
            const auto shadow_bias_matrix = depth_bias_matrix * shadow_map_mvp_matrices[i];

            const auto uname = "u_shadow_bias_matrices[" + std::to_string(i) + ']';
            const auto uloc = glGetUniformLocation(lighting_program_id, uname.data());

            glUniformMatrix4fv(uloc, 1, GL_FALSE, glm::value_ptr(shadow_bias_matrix));
        }
    }

    void shadow_map_prepass() NOEXCEPT {
        glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, framebuffer_size.x, framebuffer_size.y);

        glUseProgram(depth_program_id);
        scope_exit({ glUseProgram(0); });

        glCullFace(GL_FRONT);
        scope_exit({ glCullFace(GL_BACK); });

        const auto depth_mvp_loc = glGetUniformLocation(depth_program_id, "depth_mvp_matrix");

        for (size_t i = 0; i < lights.size(); ++i) {
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_tex_ids[i], 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUniformMatrix4fv(depth_mvp_loc, 1, GL_FALSE, glm::value_ptr(shadow_map_mvp_matrices[i]));

            glBindVertexArray(ball.mesh.vao_id);
            glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

            glBindVertexArray(plane.mesh.vao_id);
            glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);
        }
    }

    void depth_prepass() NOEXCEPT {
        glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, framebuffer_size.x, framebuffer_size.y);

        glUseProgram(depth_program_id);
        scope_exit({ glUseProgram(0); });

        glCullFace(GL_FRONT);
        scope_exit({ glCullFace(GL_BACK); });

        const auto depth_mvp_loc = glGetUniformLocation(depth_program_id, "depth_mvp_matrix");

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_tex_id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(depth_mvp_loc, 1, GL_FALSE, glm::value_ptr(transf.mvp_matrix));

        glBindVertexArray(ball.mesh.vao_id);
        glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

        glBindVertexArray(plane.mesh.vao_id);
        glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);

        transf.depth_bias_matrix = depth_bias_matrix * transf.mvp_matrix;
        update_transf_ubo();
    }

    void lighting_pass() NOEXCEPT {
        glViewport(0, 0, framebuffer_size.x, framebuffer_size.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(lighting_program_id);
        scope_exit({
            glUseProgram(0);
            glActiveTexture(GL_TEXTURE0);
        });

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_map"), 0);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_map"), 1 );

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_textured"), false);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ball.diffuse_tex_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, ball.mtl_buffer_id);
        glBindVertexArray(ball.mesh.vao_id);
        glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_textured"), true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, plane.diffuse_tex_id);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, plane.normal_tex_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, plane.mtl_buffer_id);
        glBindVertexArray(plane.mesh.vao_id);
        glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);
    }

public:
    void onContextCreated(const int fb_width, const int fb_height) {
        framebuffer_size = { fb_width, fb_height };

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glClearColor(0.2f, 0.3f, 0.8f, 1);

        ball = create_ball();
        plane = create_plane();

        lights = {
            { { -1, 1.5f, 3, 1 }, { 0.7f, 0.7f, 0.7f, 1 } },
            { { 3, 3, -2, 1 }, { 0.7f, 0.7f, 0.7f, 1 } },
        };

        create_depth_fbo();
        depth_program_id = create_depth_shader();
        lighting_program_id = create_lighting_shader();

        create_transf_ubo();
        create_lights_ubo();
        create_shadow_maps();
    }

    void onCursorMove(const double x, const double y) NOEXCEPT {
        if (!lmb_down) return;

        const auto factor = 1.0f;
        const auto diff = factor * (glm::vec2(x, y) - prev_mouse_pos);
        camera_left(diff.x);
        camera_up(-diff.y);

        look_at(eye, center, up);
        update_transf_ubo();

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onScroll(const float dx, const float dy) NOEXCEPT {
        const auto eye = this->eye * (1 - 0.1f * dy);
        const auto eye_dist = glm::length(eye);
        if (!(1 < eye_dist && eye_dist < 5)) return;

        this->eye = eye;
        look_at(eye, center, up);
        update_transf_ubo();
    }

    void onMouseButton(const int button, const int action, const int mods,
        const double x, const double y) NOEXCEPT {
        lmb_down = button == 0 && action == 1;

        if (!lmb_down) return;

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onFramebufferResize(const int width, const int height) NOEXCEPT {
        framebuffer_size = { width, height };
        look_at(eye, center, up);
        perspective(60, static_cast<float>(width) / height, 0.1f, 100.0f);
        update_transf_ubo();
    }

    void onRender() NOEXCEPT {
        static const auto shadow_map_prepassed = [=] {
            shadow_map_prepass();
            update_shadow_bias_matrices();
            return true;
        }();

        depth_prepass();
        lighting_pass();
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
