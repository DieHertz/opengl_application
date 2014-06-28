#include "ui/slider.h"
#include "ui/vertical_layout.h"
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
const auto FIRST_SHADOW_MAP_TIU = GL_TEXTURE10;
const auto SHADOW_MAP_WIDTH = 1024;
const auto SHADOW_MAP_HEIGHT = 1024;
const auto OCCLUSION_MAP_WIDTH = 640;
const auto OCCLUSION_MAP_HEIGHT = 480;
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
    scene_object scene_model;

    std::vector<light> lights;
    std::vector<glm::mat4> shadow_map_mvp_matrices;
    std::vector<GLuint> shadow_map_tex_ids;

    struct stupid_visual_cpp_compiler_does_not_perform_inplace_initialization_of_members_of_anonymous_types {
        float fovy = 60.0f;
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

    GLuint depth_fbo_id;
    GLuint depth_tex_id;
    GLuint normal_tex_id;
    GLuint depth_program_id;

    struct {
        GLuint fbo_id;
        GLuint renderbuffer_id;
        GLuint tex_id;
    } reflection;

    GLuint lighting_program_id;

    GLuint occlusion_fbo_id;
    GLuint occlusion_renderbuffer_id;
    GLuint occlusion_tex_id;
    GLuint occlusion_blurred_tex_id;
    GLuint noise_tex_id;

    GLuint occlusion_program_id;
    GLuint horizontal_blur_program_id;
    GLuint vertical_blur_program_id;

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
        glm::mat4 window_to_clip_matrix;

        std::unique_ptr<ui::widget> panel;
        struct {
            ui::slider<float>* tot_strength_slider;
            ui::slider<float>* strength_slider;
            ui::slider<int>* sample_count_slider;
            ui::slider<float>* offset_slider;
            ui::slider<float>* falloff_slider;
            ui::slider<float>* rad_slider;
        } occlusion;
        struct {
            ui::slider<int>* samples;
            ui::slider<float>* distance;
        } shadow_maps;
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
        shadow_map_tex_ids = std::vector<GLuint>(lights.size());
        glGenTextures(lights.size(), &shadow_map_tex_ids[0]);

        scope_exit({ glActiveTexture(GL_TEXTURE0); });

        for (size_t i = 0; i < lights.size(); ++i) {
            glActiveTexture(FIRST_SHADOW_MAP_TIU + i);
            glBindTexture(GL_TEXTURE_2D, shadow_map_tex_ids[i]);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT,
                0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        calculate_shadow_mvps();
    }

    void calculate_shadow_mvps() {
        shadow_map_mvp_matrices = {};

        const auto aspect_ratio = static_cast<float>(SHADOW_MAP_WIDTH) / SHADOW_MAP_HEIGHT;
        std::transform(std::begin(lights), std::end(lights), std::back_inserter(shadow_map_mvp_matrices),
            [=] (const light& l) {
                return glm::perspective(glm::radians(45.0f), aspect_ratio, 1.0f, 100.0f) *
                    glm::lookAt(glm::vec3(l.pos), camera.center, camera.up);
            }
        );
    }

    void create_depth_fbo() {
        glGenTextures(1, &depth_tex_id);
        glBindTexture(GL_TEXTURE_2D, depth_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT,
            0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &normal_tex_id);
        glBindTexture(GL_TEXTURE_2D, normal_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT,
            0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &depth_fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_tex_id, 0);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, normal_tex_id, 0);

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
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB8, SPHERE_REFLECTION_MAP_WIDTH, SPHERE_REFLECTION_MAP_HEIGHT,
                0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
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

    void create_occlusion_fbo() {
        glGenRenderbuffers(1, &occlusion_renderbuffer_id);
        glBindRenderbuffer(GL_RENDERBUFFER, occlusion_renderbuffer_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
            OCCLUSION_MAP_WIDTH, OCCLUSION_MAP_HEIGHT);

        glGenTextures(1, &occlusion_tex_id);
        glBindTexture(GL_TEXTURE_2D, occlusion_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, OCCLUSION_MAP_WIDTH, OCCLUSION_MAP_HEIGHT,
            0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glGenFramebuffers(1, &occlusion_fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, occlusion_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflection.renderbuffer_id);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, occlusion_tex_id, 0);

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

            glUniform1i(uloc, FIRST_SHADOW_MAP_TIU - GL_TEXTURE0 + i);
        }

        return program_id;
    }

    GLuint create_occlusion_shader() {
        const std::pair<const char*, GLenum> shaders[] {
            { "shaders/occlusion_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/occlusion_fragment.glsl", GL_FRAGMENT_SHADER },
        };

        const auto program_id = gl::load_shader_program(shaders);
        gl::link_shader_program(program_id);

        const auto transf_block_index = glGetUniformBlockIndex(program_id, "transformations");
        glUniformBlockBinding(program_id, transf_block_index, transf_binding_point);

        noise_tex_id = gl::load_png_texture("textures/noise.png");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glGenTextures(1, &occlusion_blurred_tex_id);
        glBindTexture(GL_TEXTURE_2D, occlusion_blurred_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, OCCLUSION_MAP_WIDTH, OCCLUSION_MAP_HEIGHT,
            0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        const std::pair<const char*, GLenum> horizontal_blur_shaders[] {
            { "shaders/occlusion_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/horizontal_blur_fragment.glsl", GL_FRAGMENT_SHADER },
        };
        horizontal_blur_program_id = gl::load_shader_program(horizontal_blur_shaders);
        gl::link_shader_program(horizontal_blur_program_id);

        const std::pair<const char*, GLenum> vertical_blur_shaders[] {
            { "shaders/occlusion_vertex.glsl", GL_VERTEX_SHADER },
            { "shaders/vertical_blur_fragment.glsl", GL_FRAGMENT_SHADER },
        };
        vertical_blur_program_id = gl::load_shader_program(vertical_blur_shaders);
        gl::link_shader_program(vertical_blur_program_id);

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

        const auto mtl = material{};

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

    void shadow_map_prepass() {
        glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);

        glUseProgram(depth_program_id);
        scope_exit({ glUseProgram(0); });

        glCullFace(GL_FRONT);
        scope_exit({ glCullFace(GL_BACK); });

        const auto depth_mvp_loc = glGetUniformLocation(depth_program_id, "depth_mvp_matrix");

        for (size_t i = 0; i < lights.size(); ++i) {
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_tex_ids[i], 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUniformMatrix4fv(depth_mvp_loc, 1, GL_FALSE, glm::value_ptr(shadow_map_mvp_matrices[i]));

            glBindVertexArray(plane.mesh.vao_id);
            glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);

            glBindVertexArray(ball.mesh.vao_id);
            glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

            glBindVertexArray(scene_model.mesh.vao_id);
            glDrawElements(scene_model.mesh.primitive_mode, scene_model.mesh.num_indices, scene_model.mesh.index_type, nullptr);
        }
    }

    void depth_prepass() {
        glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);

        glUseProgram(depth_program_id);
        scope_exit({ glUseProgram(0); });

        const auto depth_mvp_loc = glGetUniformLocation(depth_program_id, "depth_mvp_matrix");

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_tex_id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(depth_mvp_loc, 1, GL_FALSE, glm::value_ptr(transf.mvp_matrix));

        glBindVertexArray(plane.mesh.vao_id);
        glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);

        glBindVertexArray(ball.mesh.vao_id);
        glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

        glBindVertexArray(scene_model.mesh.vao_id);
        glDrawElements(scene_model.mesh.primitive_mode, scene_model.mesh.num_indices, scene_model.mesh.index_type, nullptr);

        transf.depth_bias_matrix = depth_bias_matrix * transf.mvp_matrix;
        update_transf_ubo();
    }

    void occlusion_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, occlusion_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glClearColor(1, 1, 1, 1);
        scope_exit({ glClearColor(0.2f, 0.3f, 0.8f, 1); });

        glViewport(0, 0, OCCLUSION_MAP_WIDTH, OCCLUSION_MAP_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(occlusion_program_id);
        scope_exit({
            glUseProgram(0);
            glActiveTexture(GL_TEXTURE0);
        });

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, noise_tex_id);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normal_tex_id);
        glUniform1i(glGetUniformLocation(occlusion_program_id, "u_noise_map"), 0);
        glUniform1i(glGetUniformLocation(occlusion_program_id, "u_normal_depth_map"), 1);
        glUniform1f(glGetUniformLocation(occlusion_program_id, "tot_strength"),
            ui.occlusion.tot_strength_slider->get_value());
        glUniform1f(glGetUniformLocation(occlusion_program_id, "strength"),
            ui.occlusion.strength_slider->get_value());
        glUniform1i(glGetUniformLocation(occlusion_program_id, "u_sample_count"),
            ui.occlusion.sample_count_slider->get_value());
        glUniform1f(glGetUniformLocation(occlusion_program_id, "offset"),
            ui.occlusion.offset_slider->get_value());
        glUniform1f(glGetUniformLocation(occlusion_program_id, "falloff"),
            ui.occlusion.falloff_slider->get_value());
        glUniform1f(glGetUniformLocation(occlusion_program_id, "rad"),
            ui.occlusion.rad_slider->get_value());

        glBindVertexArray(fullscreen_quad.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, 6); 
    }

    void blur_occlusion_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, occlusion_fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glViewport(0, 0, OCCLUSION_MAP_WIDTH, OCCLUSION_MAP_HEIGHT);

        glBindVertexArray(fullscreen_quad.vao_id);

        glActiveTexture(GL_TEXTURE0);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, occlusion_blurred_tex_id, 0);
        glBindTexture(GL_TEXTURE_2D, occlusion_tex_id);
        glUseProgram(horizontal_blur_program_id);
        glUniform1i(glGetUniformLocation(horizontal_blur_program_id, "u_sampler"), 0);
        glUniform1f(glGetUniformLocation(horizontal_blur_program_id, "u_size"), 1.0f / OCCLUSION_MAP_WIDTH);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, occlusion_tex_id, 0);
        glBindTexture(GL_TEXTURE_2D, occlusion_blurred_tex_id);
        glUseProgram(vertical_blur_program_id);
        glUniform1i(glGetUniformLocation(vertical_blur_program_id, "u_sampler"), 0);
        glUniform1f(glGetUniformLocation(vertical_blur_program_id, "u_size"), 1.0f / OCCLUSION_MAP_HEIGHT);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void reflection_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, reflection.fbo_id);
        scope_exit({ glBindFramebuffer(GL_FRAMEBUFFER, 0); });

        glUseProgram(lighting_program_id);
        scope_exit({ glUseProgram(0); });

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

        for (auto face = 0; face < 6; ++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                reflection.tex_id, 0);

            glViewport(0, 0, SPHERE_REFLECTION_MAP_WIDTH, SPHERE_REFLECTION_MAP_HEIGHT);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			look_at(camera.center, directions[face][0], directions[face][1]);
            update_transf_ubo();

            glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_map"), 0);
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_map"), 1 );
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_textured"), true);
            glUniform1i(glGetUniformLocation(lighting_program_id, "u_occlusion"), false);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, plane.diffuse_tex_id);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, plane.normal_tex_id);
            glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, plane.mtl_buffer_id);
            glBindVertexArray(plane.mesh.vao_id);
            glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);
        }
    }

    void lighting_pass() {
        glViewport(0, 0, framebuffer_size.x, framebuffer_size.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(lighting_program_id);
        scope_exit({
            glUseProgram(0);
            glActiveTexture(GL_TEXTURE0);
        });

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_shadow_samples"),
            ui.shadow_maps.samples->get_value());
        glUniform1f(glGetUniformLocation(lighting_program_id, "u_shadow_distance"),
            ui.shadow_maps.distance->get_value());

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, occlusion_tex_id);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_diffuse_map"), 0);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_normal_map"), 1 );
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_occlusion_map"), 3);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_occlusion"), true);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_textured"), true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, plane.diffuse_tex_id);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, plane.normal_tex_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, plane.mtl_buffer_id);
        glBindVertexArray(plane.mesh.vao_id);
        glDrawElements(plane.mesh.primitive_mode, plane.mesh.num_indices, plane.mesh.index_type, nullptr);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_textured"), true);
        glUniform1i(glGetUniformLocation(lighting_program_id, "u_reflection_map"), 2);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ball.diffuse_tex_id);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflection.tex_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, ball.mtl_buffer_id);
        glBindVertexArray(ball.mesh.vao_id);
        glDrawElements(ball.mesh.primitive_mode, ball.mesh.num_indices, ball.mesh.index_type, nullptr);

        glUniform1i(glGetUniformLocation(lighting_program_id, "u_textured"), false);
        glBindVertexArray(scene_model.mesh.vao_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, mtl_binding_point, scene_model.mtl_buffer_id);
        glDrawElements(scene_model.mesh.primitive_mode, scene_model.mesh.num_indices, scene_model.mesh.index_type, nullptr);
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
        glViewport(0, 0, framebuffer_size.x, framebuffer_size.y);

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

        ui.panel->draw();
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
        ui.window_to_clip_matrix = glm::ortho(
            0.0f, window_size.x,
            window_size.y, 0.0f
        );
        glUseProgram(ui.program_id);
        scope_exit({ glUseProgram(0); });
        glUniformMatrix4fv(glGetUniformLocation(ui.program_id, "window_to_clip_matrix"),
            1, GL_FALSE, glm::value_ptr(ui.window_to_clip_matrix));
    }

    void create_ui() {
        ui.program_id = create_ui_shader();
        update_ui_transform();

        ui.panel.reset(new ui::widget{});

        auto vlayout = new ui::vertical_layout{ui.panel.get()};
        vlayout->set_offset(5);

        ui.occlusion.tot_strength_slider = new ui::slider<float>{vlayout};
        ui.occlusion.tot_strength_slider->set_size(150, 15);
        vlayout->add_widget(ui.occlusion.tot_strength_slider);
        ui.occlusion.tot_strength_slider->set_min_max(0.1f, 10.0f, 3.0f);

        ui.occlusion.strength_slider = new ui::slider<float>{vlayout};
        ui.occlusion.strength_slider->set_size(150, 15);
        vlayout->add_widget(ui.occlusion.strength_slider);
        ui.occlusion.strength_slider->set_min_max(0.01f, 1.0f, 0.015f);

        ui.occlusion.sample_count_slider = new ui::slider<int>{vlayout};
        ui.occlusion.sample_count_slider->set_size(150, 15);
        vlayout->add_widget(ui.occlusion.sample_count_slider);
        ui.occlusion.sample_count_slider->set_min_max(1, 16, 16);

        ui.occlusion.offset_slider = new ui::slider<float>{vlayout};
        ui.occlusion.offset_slider->set_size(150, 15);
        vlayout->add_widget(ui.occlusion.offset_slider);
        ui.occlusion.offset_slider->set_min_max(1.0f, 30.0f, 30.0f);

        ui.occlusion.falloff_slider = new ui::slider<float>{vlayout};
        ui.occlusion.falloff_slider->set_size(150, 15);
        vlayout->add_widget(ui.occlusion.falloff_slider);
        ui.occlusion.falloff_slider->set_min_max(0, 0.01f, 0.000002f);

        ui.occlusion.rad_slider = new ui::slider<float>{vlayout};
        ui.occlusion.rad_slider->set_size(150, 15);
        vlayout->add_widget(ui.occlusion.rad_slider);
        ui.occlusion.rad_slider->set_min_max(0.001f, 0.01f, 0.001f);

        ui.shadow_maps.samples = new ui::slider<int>{vlayout};
        ui.shadow_maps.samples->set_size(150, 15);
        vlayout->add_widget(ui.shadow_maps.samples);
        ui.shadow_maps.samples->set_min_max(0, 16, 8);

        ui.shadow_maps.distance = new ui::slider<float>{vlayout};
        ui.shadow_maps.distance->set_size(150, 15);
        vlayout->add_widget(ui.shadow_maps.distance);
        ui.shadow_maps.distance->set_min_max(100.0f, 1000.0f, 600.0f);
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
        scene_model = {
			mesh::load_mdl("models/ssao-test-scene.mdl"),
            material{ { 0.707f, 0.707f, 0.707f, 1 }, { 0, 0, 0, 1 }, 0, 0 }
        };
        glGenBuffers(1, &scene_model.mtl_buffer_id);
        glBindBuffer(GL_UNIFORM_BUFFER, scene_model.mtl_buffer_id);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(scene_model.mtl), &scene_model.mtl, GL_DYNAMIC_DRAW);

        lights = {
            { { 20, 30, 10, 1 }, { 0.7f, 0.7f, 0.7f, 1 } },
            { { 20, 20, -15, 1 }, { 0.7f, 0.7f, 0.7f, 1 } },
        };

        create_depth_fbo();
        create_occlusion_fbo();
        create_reflection_fbo();

        depth_program_id = create_depth_shader();
        lighting_program_id = create_lighting_shader();
        occlusion_program_id = create_occlusion_shader();

        create_transf_ubo();
        create_lights_ubo();
        create_shadow_maps();

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
        perspective(camera.fovy, static_cast<float>(width) / height, 0.1f, 100.0f);
        update_transf_ubo();
        update_ui_transform();
    }

    void onUpdate(const float now, const float elapsed) {
        lights[0].pos.x = 20 * std::cos(0.25f * now);
        lights[0].pos.z = 10 * std::sin(0.25f * now);

        update_lights_ubo();
        calculate_shadow_mvps();

        shadow_maps_require_update = true;
    }

    void onRender() {
        if (shadow_maps_require_update) {
            shadow_map_prepass();
            update_shadow_bias_matrices();
        }

        depth_prepass();
        occlusion_pass();
        blur_occlusion_pass();
        reflection_pass();
        lighting_pass();

        // debug.draw(occlusion_tex_id, { 0, 0, framebuffer_size.x / 2, framebuffer_size.y / 2 });
        // debug.draw(normal_tex_id, { 3 * framebuffer_size.x / 4, 0, framebuffer_size.x / 4, framebuffer_size.y / 4 });

        draw_ui();
    }
};

int main() {
    try {
        opengl_application<handler> app{4, 1, 640, 480};
        app.run();
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
}
