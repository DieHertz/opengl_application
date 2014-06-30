#include "ui/slider.h"
#include "ui/vertical_layout.h"
#include "ui/text.h"
#include "opengl_application.h"
#include "debug_surface.h"
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
const auto FIRST_SM_TIU = GL_TEXTURE10;
const auto SM_WIDTH = 1024;
const auto SM_HEIGHT = 1024;
const auto SSAO_MAP_WIDTH = 640;
const auto SSAO_MAP_HEIGHT = 480;
const auto SPHERE_REFLECTION_MAP_WIDTH = 256;
const auto SPHERE_REFLECTION_MAP_HEIGHT = 256;

struct material {
    glm::vec4 diffuse;
    glm::vec4 specular;
    float shininess;
    float reflectance;
};

struct scene_object {
    mesh::mesh_data mesh;
    material mtl;
    GLuint mtl_buffer_id;
    GLuint diffuse_tex_id;
    GLuint normal_tex_id;
    GLuint height_tex_id;
};

struct light {
    glm::vec4 pos;
    glm::vec4 color;
};

struct transformations {
    glm::mat4 depth_bias_matrix;
    glm::mat4 mv_matrix;
    glm::mat4 mvp_matrix;
    glm::mat4 projection_matrix;
    glm::mat4 normal_matrix;
};

class handler {
    scene_object ball;
    scene_object plane;

    std::vector<light> lights;

    struct stupid_visual_cpp_compiler_does_not_perform_inplace_initialization_of_members_of_anonymous_types {
        float fovy = 60.0f;
        float near = 0.1f;
        float far = 100.0f;
        glm::vec3 eye{-3, 1.5f, 1};
        glm::vec3 center{0, 0, 0};
        glm::vec3 up{0, 1, 0};
    } camera;

    const glm::mat4 depth_bias_matrix{
        0.5f,   0,      0,      0,
        0,      0.5f,   0,      0,
        0,      0,      0.5f,   0,
        0.5f,   0.5f,   0.5f,   1
    };

    glm::mat4 model, view;
    transformations transf;

    GLuint transf_buffer_id;
    const GLuint transf_binding_point = 1;

    GLuint lights_buffer_id;
    const GLuint lights_binding_point = 2;

    const GLuint mtl_binding_point = 3;

    struct {
        GLuint fbo_id;
        GLuint renderbuffer_id;
        GLuint tex_id;
        GLuint program_id;
    } depth;

    struct {
        GLuint fbo_id;
        GLuint renderbuffer_id;
        GLuint tex_id, blurred_tex_id, noise_tex_id;
        GLuint program_id, hblur_program_id, vblur_program_id;
    } ssao;

    struct {
        GLuint fbo_id;
        std::vector<GLuint> tex_ids;
        std::vector<glm::mat4> mvp_matrices;
        GLuint program_id;
    } sm;

    struct {
        GLuint fbo_id;
        GLuint renderbuffer_id;
        GLuint tex_id;
    } reflection;

    GLuint skybox_tex_id;
    GLuint lighting_program_id;

    bool shadow_maps_require_update = true;

    bool camera_dragging = false;
    glm::vec2 prev_mouse_pos;

    glm::vec2 framebuffer_size;
    glm::vec2 window_size;

    debug_surface debug;

    struct {
        GLuint vao_id, vbo_id;
    } fullscreen_quad;

    struct {
        GLuint program_id;
        glm::mat4 transform;
        std::shared_ptr<ui::font> p_font;

        std::unique_ptr<ui::widget> panel;
        struct {
            ui::slider<float>* total_strength;
            ui::slider<float>* strength;
            ui::slider<int>* sample_count;
            ui::slider<float>* offset;
            ui::slider<float>* falloff;
            ui::slider<float>* radius;
            ui::slider<float>* depth_bias;
            ui::slider<float>* hblur_size;
            ui::slider<float>* vblur_size;
        } ssao;
        struct {
            ui::slider<int>* samples;
            ui::slider<float>* distance;
        } sm;
        struct {
            ui::slider<float>* scale;
            ui::slider<float>* bias;
        } parallax;

        ui::text* fps;
    } ui;

    void look_at(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        view = glm::lookAt(eye, center, up);
        transf.mv_matrix = view * model;
        transf.normal_matrix = glm::transpose(glm::inverse(transf.mv_matrix));
        transf.mvp_matrix = transf.projection_matrix * transf.mv_matrix;
    }

    void perspective(const float fovy, const float aspect, const float zNear, const float zFar) {
        transf.projection_matrix = glm::perspective(glm::radians(fovy), aspect, zNear, zFar);
        transf.mvp_matrix = transf.projection_matrix * transf.mv_matrix;
    }

    void camera_up(const float degrees) {
        const auto ortho = glm::normalize(glm::cross(camera.eye, camera.up));

        camera.eye = glm::vec3(glm::vec4(camera.eye, 1) * glm::rotate(glm::mat4(), glm::radians(degrees), ortho));
        // up = glm::vec3(glm::vec4(up, 0) * glm::rotate(glm::mat4(), glm::radians(degrees), ortho));
    }

    void camera_left(const float degrees) {
        const auto eye = glm::vec4(camera.eye, 1) * glm::rotate(glm::mat4(), glm::radians(degrees), camera.up);

        camera.eye = glm::vec3(eye);
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

    void update_lights_ubo() {
        glBindBuffer(GL_UNIFORM_BUFFER, lights_buffer_id);

        const auto num_lights_offset = MAX_LIGHTS * sizeof(lights.front());
        const auto num_lights = GLint(lights.size());
        glBufferSubData(GL_UNIFORM_BUFFER, 0, lights.size() * sizeof(lights.front()), lights.data());
        glBufferSubData(GL_UNIFORM_BUFFER, num_lights_offset, sizeof(num_lights), &num_lights);
    }

    void create_shadow_maps() {
        sm.tex_ids = std::vector<GLuint>(lights.size());
        glGenTextures(lights.size(), &sm.tex_ids[0]);

        scope_exit({ glActiveTexture(GL_TEXTURE0); });

        GLfloat border_color[] { 1.0f, 0, 0, 0 };
        for (size_t i = 0; i < lights.size(); ++i) {
            glActiveTexture(FIRST_SM_TIU + i);
            glBindTexture(GL_TEXTURE_2D, sm.tex_ids[i]);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SM_WIDTH, SM_HEIGHT,
                0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        }

        calculate_shadow_mvps();
    }

    void calculate_shadow_mvps() {
        sm.mvp_matrices = {};

        const auto aspect_ratio = static_cast<float>(SM_WIDTH) / SM_HEIGHT;
        std::transform(std::begin(lights), std::end(lights), std::back_inserter(sm.mvp_matrices),
            [=] (const light& l) {
                return glm::perspective(glm::radians(45.0f), aspect_ratio, 1.0f, 100.0f) *
                    glm::lookAt(glm::vec3(l.pos), camera.center, camera.up);
            }
        );
    }

    void create_depth_fbo() {
        glGenRenderbuffers(1, &depth.renderbuffer_id);
        glBindRenderbuffer(GL_RENDERBUFFER, depth.renderbuffer_id);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
            SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT
        );

        glGenTextures(1, &depth.tex_id);
        glBindTexture(GL_TEXTURE_2D, depth.tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT,
            0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &depth.fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, depth.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth.renderbuffer_id);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, depth.tex_id, 0);

        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error{"create_depth_fbo() failed"};
        }
    }

    void create_reflection_fbo() {
        glGenRenderbuffers(1, &reflection.renderbuffer_id);
        glBindRenderbuffer(GL_RENDERBUFFER, reflection.renderbuffer_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
            SPHERE_REFLECTION_MAP_WIDTH, SPHERE_REFLECTION_MAP_HEIGHT);

        glGenTextures(1, &reflection.tex_id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflection.tex_id);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        for (auto face = 0; face < 6; ++face) {
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB8,
                SPHERE_REFLECTION_MAP_WIDTH, SPHERE_REFLECTION_MAP_HEIGHT,
                0, GL_RGB, GL_UNSIGNED_BYTE, nullptr
            );
        }

        glGenFramebuffers(1, &reflection.fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, reflection.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflection.renderbuffer_id);

        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error{"create_reflection_fbo() failed"};
        }
    }

    void create_shadow_maps_fbo() {
        glGenFramebuffers(1, &sm.fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, sm.fbo_id);
        glDrawBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void create_ssao_fbo() {
        glGenRenderbuffers(1, &ssao.renderbuffer_id);
        glBindRenderbuffer(GL_RENDERBUFFER, ssao.renderbuffer_id);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
            SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT
        );

        glGenTextures(1, &ssao.tex_id);
        glBindTexture(GL_TEXTURE_2D, ssao.tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT,
            0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glGenFramebuffers(1, &ssao.fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, ssao.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflection.renderbuffer_id);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ssao.tex_id, 0);

        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error{"create_reflection_fbo() failed"};
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
        if (transf_block_index != GL_INVALID_INDEX) {
            glUniformBlockBinding(program_id, transf_block_index, transf_binding_point);
        }

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

            glUniform1i(uloc, FIRST_SM_TIU - GL_TEXTURE0 + i);
        }

        return program_id;
    }

    GLuint create_ssao_shader() {
        const std::pair<const char*, GLenum> shaders[] {
            { "shaders/occlusion_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/occlusion_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        const auto transf_block_index = glGetUniformBlockIndex(program_id, "transformations");
        glUniformBlockBinding(program_id, transf_block_index, transf_binding_point);

        ssao.noise_tex_id = gl::load_png_texture("textures/noise.png");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glGenTextures(1, &ssao.blurred_tex_id);
        glBindTexture(GL_TEXTURE_2D, ssao.blurred_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT,
            0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        const std::pair<const char*, GLenum> horizontal_blur_shaders[] {
            { "shaders/occlusion_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/horizontal_blur_fragment.glsl", GL_FRAGMENT_SHADER },
        };
        ssao.hblur_program_id = gl::load_shader_program(horizontal_blur_shaders);
        gl::link_shader_program(ssao.hblur_program_id);

        const std::pair<const char*, GLenum> vertical_blur_shaders[] {
            { "shaders/occlusion_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/vertical_blur_fragment.glsl", GL_FRAGMENT_SHADER },
        };
        ssao.vblur_program_id = gl::load_shader_program(vertical_blur_shaders);
        gl::link_shader_program(ssao.vblur_program_id);

        return program_id;
    }

    GLuint create_sm_shader() {
        const std::pair<const char*, GLenum> shaders[] {
            { "shaders/shadow_map_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/shadow_map_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        return program_id;
    }

    scene_object create_ball() {
        const auto mesh = mesh::gen_sphere(0.5f, 32, 32);

        const auto diffuse_tex_id = gl::load_png_texture("textures/ball12_diffuse.png");

        const auto mtl = material{ { 0, 0, 0, 1 }, { 1, 1, 1, 1 }, 200, 0.25f };

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

        const auto height_tex_id = gl::load_png_texture("textures/table_cloth_height.png");
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        const auto mtl = material{};

        auto mtl_buffer_id = GLuint{};
        glGenBuffers(1, &mtl_buffer_id);
        glBindBuffer(GL_UNIFORM_BUFFER, mtl_buffer_id);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(mtl), &mtl, GL_DYNAMIC_DRAW);

        return { mesh, mtl, mtl_buffer_id, diffuse_tex_id, normal_tex_id, height_tex_id };
    }

    void update_shadow_bias_matrices() {
        glUseProgram(lighting_program_id);

        for (size_t i = 0; i < lights.size(); ++i) {
            const auto shadow_bias_matrix = depth_bias_matrix * sm.mvp_matrices[i];

            const auto uname = "u_shadow_bias_matrices[" + std::to_string(i) + ']';
            const auto uloc = glGetUniformLocation(lighting_program_id, uname.data());

            glUniformMatrix4fv(uloc, 1, GL_FALSE, glm::value_ptr(shadow_bias_matrix));
        }
    }

    void sm_prepass() {
        glBindFramebuffer(GL_FRAMEBUFFER, sm.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, SM_WIDTH, SM_HEIGHT);

        glUseProgram(sm.program_id);
        scope_exit({ glUseProgram(0); });

        glCullFace(GL_FRONT);
        scope_exit({ glCullFace(GL_BACK); });

        const auto depth_mvp_loc = glGetUniformLocation(sm.program_id, "depth_mvp_matrix");

        for (size_t i = 0; i < lights.size(); ++i) {
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, sm.tex_ids[i], 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUniformMatrix4fv(depth_mvp_loc, 1, GL_FALSE, glm::value_ptr(sm.mvp_matrices[i]));

            glBindVertexArray(plane.mesh.vao_id);
            glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);

            glBindVertexArray(ball.mesh.vao_id);
            glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);
        }
    }

    void depth_prepass() {
        glBindFramebuffer(GL_FRAMEBUFFER, depth.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(depth.program_id);
        scope_exit({ glUseProgram(0); });

        glUniform1f(glGetUniformLocation(depth.program_id, "u_near"), camera.near);
        glUniform1f(glGetUniformLocation(depth.program_id, "u_far"), camera.far);

        glBindVertexArray(plane.mesh.vao_id);
        glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);

        glBindVertexArray(ball.mesh.vao_id);
        glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

        transf.depth_bias_matrix = depth_bias_matrix * transf.mvp_matrix;
        update_transf_ubo();
    }

    void ssao_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, ssao.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glClearColor(1, 1, 1, 1);
        scope_exit({ glClearColor(0.2f, 0.3f, 0.8f, 1); });

        glViewport(0, 0, SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ssao.program_id);
        scope_exit({
            glUseProgram(0);
            glActiveTexture(GL_TEXTURE0);
        });

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssao.noise_tex_id);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depth.tex_id);
        glUniform1i(glGetUniformLocation(ssao.program_id, "u_noise_map"), 0);
        glUniform1i(glGetUniformLocation(ssao.program_id, "u_normal_depth_map"), 1);
        glUniform1f(glGetUniformLocation(ssao.program_id, "u_total_strength"),
            ui.ssao.total_strength->get_value());
        glUniform1f(glGetUniformLocation(ssao.program_id, "u_strength"),
            ui.ssao.strength->get_value());
        glUniform1i(glGetUniformLocation(ssao.program_id, "u_sample_count"),
            ui.ssao.sample_count->get_value());
        glUniform1f(glGetUniformLocation(ssao.program_id, "u_offset"),
            ui.ssao.offset->get_value());
        glUniform1f(glGetUniformLocation(ssao.program_id, "u_falloff"),
            ui.ssao.falloff->get_value());
        glUniform1f(glGetUniformLocation(ssao.program_id, "u_radius"),
            ui.ssao.radius->get_value());
        glUniform1f(glGetUniformLocation(ssao.program_id, "u_depth_bias"),
            ui.ssao.depth_bias->get_value());

        glBindVertexArray(fullscreen_quad.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, 6); 
    }

    void blur_ssao_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, ssao.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, SSAO_MAP_WIDTH, SSAO_MAP_HEIGHT);

        glBindVertexArray(fullscreen_quad.vao_id);

        glActiveTexture(GL_TEXTURE0);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ssao.blurred_tex_id, 0);
        glBindTexture(GL_TEXTURE_2D, ssao.tex_id);
        glUseProgram(ssao.hblur_program_id);
        glUniform1i(glGetUniformLocation(ssao.hblur_program_id, "u_sampler"), 0);
        glUniform1f(glGetUniformLocation(ssao.hblur_program_id, "u_size"),
            ui.ssao.hblur_size->get_value() / SSAO_MAP_WIDTH
        );

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ssao.tex_id, 0);
        glBindTexture(GL_TEXTURE_2D, ssao.blurred_tex_id);
        glUseProgram(ssao.vblur_program_id);
        glUniform1i(glGetUniformLocation(ssao.vblur_program_id, "u_sampler"), 0);
        glUniform1f(glGetUniformLocation(ssao.vblur_program_id, "u_size"),
            ui.ssao.vblur_size->get_value() / SSAO_MAP_HEIGHT
        );

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void reflection_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, reflection.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glUseProgram(lighting_program_id);
        scope_exit({ glUseProgram(0); });

        glUniform4fv(glGetUniformLocation(lighting_program_id, "u_camera_pos_worldspace"), 1,
            glm::value_ptr(camera.center));

        const auto old_transf = transf;
        scope_exit({
            transf = old_transf;
            update_transf_ubo();
            glActiveTexture(GL_TEXTURE0);
        });

        perspective(90.0f, static_cast<float>(SPHERE_REFLECTION_MAP_WIDTH) / SPHERE_REFLECTION_MAP_HEIGHT,
            0.1f, 50.0f);

        const glm::vec3 directions[][2] {
            { { 1, 0, 0 },  { 0, -1, 0 } },
            { { -1, 0, 0 }, { 0, -1, 0 } },
            { { 0, 1, 0 },  { 0, 0, -1 } },
            { { 0, -1, 0 }, { 0, 0, -1 } },
            { { 0, 0, 1 },  { 0, -1, 0 } },
            { { 0, 0, -1 }, { 0, -1, 0 } },
        };

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ssao.tex_id);

        for (auto face = 0; face < 6; ++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                reflection.tex_id, 0);

            glViewport(0, 0, SPHERE_REFLECTION_MAP_WIDTH, SPHERE_REFLECTION_MAP_HEIGHT);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			look_at(camera.center, directions[face][0], directions[face][1]);
            update_transf_ubo();

            glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_map"), 0);
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_map"), 1 );
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_occlusion_map"), 2);
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_height_map"), 4);
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_textured"), true);
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_textured"), plane.normal_tex_id != 0);
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_occlusion"), false);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, plane.diffuse_tex_id);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, plane.normal_tex_id);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, plane.height_tex_id);
            glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, plane.mtl_buffer_id);
            glBindVertexArray(plane.mesh.vao_id);
            glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);
        }
    }

    void lighting_pass() {
        glViewport(0, 0, static_cast<int>(framebuffer_size.x), static_cast<int>(framebuffer_size.y));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(lighting_program_id);
        scope_exit({
            glUseProgram(0);
            glActiveTexture(GL_TEXTURE0);
        });

        glUniform4fv(glGetUniformLocation(lighting_program_id, "u_camera_pos_worldspace"), 1,
            glm::value_ptr(camera.eye));

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_shadow_samples"),
            ui.sm.samples->get_value());
        glUniform1f(glGetUniformLocation(lighting_program_id, "u_shadow_distance"),
            ui.sm.distance->get_value());

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ssao.tex_id);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_map"), 0);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_map"), 1 );
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_occlusion_map"), 2);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_reflection_map"), 3);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_height_map"), 4);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_occlusion"), true);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_textured"), true);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_textured"), plane.normal_tex_id != 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, plane.diffuse_tex_id);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, plane.normal_tex_id);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, plane.height_tex_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, plane.mtl_buffer_id);
        glBindVertexArray(plane.mesh.vao_id);
        glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_textured"), true);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_textured"), ball.normal_tex_id != 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ball.diffuse_tex_id);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflection.tex_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, ball.mtl_buffer_id);
        glBindVertexArray(ball.mesh.vao_id);
        glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);
    }

    GLuint create_ui_shader() {
        const std::pair<const char*, GLenum> shaders[] {
            { "shaders/ui_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/ui_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        return program_id;
    }

    void draw_ui() {
		glViewport(0, 0, static_cast<int>(framebuffer_size.x), static_cast<int>(framebuffer_size.y));

        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_ALWAYS);
        scope_exit({
            glEnable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glDepthFunc(GL_LESS);
        });

        glUseProgram(ui.program_id);
        ui.panel->draw(ui.program_id);
    }

    void create_fullscreen_quad() {
        const GLfloat vertices[] {
            -1, 1,
            -1, -1,
            1, -1,
            -1, 1,
            1, -1,
            1, 1
        };

        glGenVertexArrays(1, &fullscreen_quad.vao_id);
        glBindVertexArray(fullscreen_quad.vao_id);

        glGenBuffers(1, &fullscreen_quad.vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, fullscreen_quad.vbo_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(vertices[0]), 0);
    }

    void update_ui_transform() {
        ui.transform = glm::ortho(
            0.0f, window_size.x,
            window_size.y, 0.0f
        );
        glUseProgram(ui.program_id);
        scope_exit({ glUseProgram(0); });
        glUniformMatrix4fv(glGetUniformLocation(ui.program_id, "u_transform"),
            1, GL_FALSE, glm::value_ptr(ui.transform));
    }

    void create_ui() {
        ui.program_id = create_ui_shader();
        update_ui_transform();

        std::ifstream font_file("fonts/sourcecodepro-light.fnt");
        ui.p_font = std::make_shared<ui::font>(font_file);

        ui.panel.reset(new ui::widget{});

        auto vlayout = new ui::vertical_layout{ui.panel.get()};
        vlayout->set_pos(5, 0);
        vlayout->set_offset(5);

        vlayout->add_widget(new ui::text{"SSAO", ui.p_font, vlayout});

        ui.ssao.total_strength = new ui::slider<float>{"total_strength", ui.p_font, vlayout};
        ui.ssao.total_strength->set_size(150, 15);
        vlayout->add_widget(ui.ssao.total_strength);
        ui.ssao.total_strength->set_min_max(0.1f, 10.0f, 1.5f);

        ui.ssao.strength = new ui::slider<float>{"strength", ui.p_font, vlayout};
        ui.ssao.strength->set_size(150, 15);
        vlayout->add_widget(ui.ssao.strength);
        ui.ssao.strength->set_min_max(0.01f, 1.0f, 0.15f);

        ui.ssao.sample_count = new ui::slider<int>{"sample_count", ui.p_font, vlayout};
        ui.ssao.sample_count->set_size(150, 15);
        vlayout->add_widget(ui.ssao.sample_count);
        ui.ssao.sample_count->set_min_max(0, 16, 16);

        ui.ssao.offset = new ui::slider<float>{"offset", ui.p_font, vlayout};
        ui.ssao.offset->set_size(150, 15);
        vlayout->add_widget(ui.ssao.offset);
        ui.ssao.offset->set_min_max(1.0f, 30.0f, 30.0f);

        ui.ssao.falloff = new ui::slider<float>{"falloff", ui.p_font, vlayout};
        ui.ssao.falloff->set_size(150, 15);
        vlayout->add_widget(ui.ssao.falloff);
        ui.ssao.falloff->set_min_max(0, 0.01f, 0.000002f);

        ui.ssao.radius = new ui::slider<float>{"radius", ui.p_font, vlayout};
        ui.ssao.radius->set_size(150, 15);
        vlayout->add_widget(ui.ssao.radius);
        ui.ssao.radius->set_min_max(0.001f, 0.01f, 0.001f);

        ui.ssao.depth_bias = new ui::slider<float>{"depth_bias", ui.p_font, vlayout};
        ui.ssao.depth_bias->set_size(150, 15);
        vlayout->add_widget(ui.ssao.depth_bias);
        ui.ssao.depth_bias->set_min_max(0, 0.5f, 0.005f);

        ui.ssao.hblur_size = new ui::slider<float>{"hblur_size", ui.p_font, vlayout};
        ui.ssao.hblur_size->set_size(150, 15);
        vlayout->add_widget(ui.ssao.hblur_size);
        ui.ssao.hblur_size->set_min_max(0.1f, 2.0f, 1.0f);

        ui.ssao.vblur_size = new ui::slider<float>{"vblur_size", ui.p_font, vlayout};
        ui.ssao.vblur_size->set_size(150, 15);
        vlayout->add_widget(ui.ssao.vblur_size);
        ui.ssao.vblur_size->set_min_max(0.1f, 2.0f, 1.0f);

        vlayout->add_widget(new ui::text{"SM", ui.p_font, vlayout});

        ui.sm.samples = new ui::slider<int>{"sample_count", ui.p_font, vlayout};
        ui.sm.samples->set_size(150, 15);
        vlayout->add_widget(ui.sm.samples);
        ui.sm.samples->set_min_max(0, 16, 8);

        ui.sm.distance = new ui::slider<float>{"poisson_distance", ui.p_font, vlayout};
        ui.sm.distance->set_size(150, 15);
        vlayout->add_widget(ui.sm.distance);
        ui.sm.distance->set_min_max(100.0f, 1000.0f, 600.0f);

        vlayout->add_widget(new ui::text{"parallax mapping", ui.p_font, vlayout});

        ui.parallax.scale = new ui::slider<float>{"scale", ui.p_font, vlayout};
        ui.parallax.scale->set_size(150, 15);
        vlayout->add_widget(ui.parallax.scale);
        ui.parallax.scale->set_min_max(0, 0.1f, 0.005f);
        ui.parallax.scale->on_change([this] (const float scale) {
            glUseProgram(lighting_program_id);
            glUniform1f(glGetUniformLocation(lighting_program_id, "u_parallax_scale"), scale);
            glUseProgram(0);
        });

        ui.parallax.bias = new ui::slider<float>{"bias", ui.p_font, vlayout};
        ui.parallax.bias->set_size(150, 15);
        vlayout->add_widget(ui.parallax.bias);
        ui.parallax.bias->set_min_max(0, 0.1f, 0);
        ui.parallax.bias->on_change([this] (const float bias) {
            glUseProgram(lighting_program_id);
            glUniform1f(glGetUniformLocation(lighting_program_id, "u_parallax_bias"), bias);
            glUseProgram(0);
		});

        ui.fps = new ui::text{"", ui.p_font, ui.panel.get()};
        ui.fps->set_pos(600, 0);
    }

public:
    void onContextCreated(
        const int fb_width, const int fb_height,
        const int window_width, const int window_height
    ) {
        framebuffer_size = { fb_width, fb_height };
        window_size = { window_width, window_height };

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glClearColor(0.2f, 0.3f, 0.8f, 1);

        debug.init();

        ball = create_ball();
        plane = create_plane();
        // scene_model = {
        //     mesh::load_mdl("models/ssao-test-scene.mdl"),
        //     material{ { 0.707f, 0.707f, 0.707f, 1 }, { 0, 0, 0, 1 }, 0, 0 }
        // };
        // glGenBuffers(1, &scene_model.mtl_buffer_id);
        // glBindBuffer(GL_UNIFORM_BUFFER, scene_model.mtl_buffer_id);
        // glBufferData(GL_UNIFORM_BUFFER, sizeof(scene_model.mtl), &scene_model.mtl, GL_DYNAMIC_DRAW);

        lights = {
            { { 20, 30, 10, 1 }, { 0.7f, 0.7f, 0.7f, 1 } },
            { { 20, 20, -15, 1 }, { 0.7f, 0.7f, 0.7f, 1 } },
        };

        create_depth_fbo();
        create_ssao_fbo();
        create_reflection_fbo();
        create_shadow_maps_fbo();

        depth.program_id = create_depth_shader();
        ssao.program_id = create_ssao_shader();
        sm.program_id = create_sm_shader();
        lighting_program_id = create_lighting_shader();

        create_transf_ubo();
        create_lights_ubo();
        create_shadow_maps();

        skybox_tex_id = gl::load_png_texture_cube("textures/skybox");

        create_fullscreen_quad();

        create_ui();
    }

    void onCursorMove(const float x, const float y) {
        if (ui.panel->on_mouse_move(x, y)) return;

        if (!camera_dragging) return;

        const auto factor = 1.0f;
        const auto diff = factor * (glm::vec2(x, y) - prev_mouse_pos);
        camera_left(diff.x);
        camera_up(-diff.y);

		look_at(camera.eye, camera.center, camera.up);
        update_transf_ubo();

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onScroll(const float dx, const float dy) {
		const auto dir = camera.eye - camera.center;
        const auto dir_length = glm::length(dir);
        const auto scale = 1 - 0.1f * dy;
        const auto length = glm::clamp(dir_length * scale, 1.0f, 50.0f);

		camera.eye = camera.center + length / dir_length * dir;
		look_at(camera.eye, camera.center, camera.up);
        update_transf_ubo();
    }

    void onMouseButton(const int button, const int action, const int mods,
        const float x, const float y) {
        if (action == 1 && ui.panel->on_mouse_down(button, mods, x, y)) return;
        else if (action == 0 && ui.panel->on_mouse_up(button, mods, x, y)) return;

        camera_dragging = button == 2 && action == 1;

        if (!camera_dragging) return;

        prev_mouse_pos = glm::vec2(x, y);
    }

    void onFramebufferResize(
        const int width, const int height,
        const int window_width, const int window_height
    ) {
        framebuffer_size = { width, height };
        window_size = { window_width, window_height };

		look_at(camera.eye, camera.center, camera.up);
        perspective(camera.fovy, static_cast<float>(width) / height, camera.near, camera.far);
        update_transf_ubo();
        update_ui_transform();
    }

    void onUpdate(const float now, const float elapsed) {
        lights[0].pos.x = 20 * std::cos(0.25f * now);
        lights[0].pos.z = 10 * std::sin(0.25f * now);

        update_lights_ubo();
        calculate_shadow_mvps();

        shadow_maps_require_update = true;

        ui.fps->set_string(std::to_string(static_cast<int>(1.0f / elapsed)));
    }

    void onRender() {
        if (shadow_maps_require_update) {
            sm_prepass();
            update_shadow_bias_matrices();
        }

        depth_prepass();
        ssao_pass();
        blur_ssao_pass();
        reflection_pass();
        lighting_pass();

        // debug.draw(ssao.tex_id, { 0, 0, framebuffer_size.x / 2, framebuffer_size.y / 2 });

        draw_ui();
    }
};

int main() {
    try {
        opengl_application<handler> app{4, 1, 640, 480, 4};
        app.run();
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
}
