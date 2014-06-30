#ifndef lighting_program_h
#define lighting_program_h

#include "program_common.h"
#include "scope_exit.h"
#include "gl/util.h"
#include <string>

struct lighting_program {
    GLuint id;

    GLuint diffuse_textured_loc;
    GLuint normal_textured_loc;
    GLuint diffuse_map_loc;
    GLuint normal_map_loc;
    GLuint height_map_loc;
    GLuint occlusion_map_loc;
    GLuint reflection_map_loc;
    GLuint camera_pos_worldspace_loc;
    GLuint parallax_scale_loc;
    GLuint parallax_bias_loc;
    GLuint shadow_samples_loc;
    GLuint shadow_distance_loc;
    GLuint depth_bias_loc;
};

namespace {

const struct {
    const char* name;
    GLuint lighting_program::* ptr;
} lighting_program_uniforms[] {
    { "u_diffuse_textured", &lighting_program::diffuse_textured_loc },
    { "u_normal_textured", &lighting_program::normal_textured_loc },
    { "u_diffuse_map", &lighting_program::diffuse_map_loc },
    { "u_normal_map", &lighting_program::normal_map_loc },
    { "u_height_map", &lighting_program::height_map_loc },
    { "u_occlusion_map", &lighting_program::occlusion_map_loc },
    { "u_reflection_map", &lighting_program::reflection_map_loc },
    { "u_camera_pos_worldspace", &lighting_program::camera_pos_worldspace_loc },
    { "u_parallax_scale", &lighting_program::parallax_scale_loc },
    { "u_parallax_bias", &lighting_program::parallax_bias_loc },
    { "u_shadow_samples", &lighting_program::shadow_samples_loc },
    { "u_shadow_distance", &lighting_program::shadow_distance_loc },
    { "u_depth_bias", &lighting_program::depth_bias_loc },
};

} 

lighting_program create_lighting_program() {
    auto program = lighting_program{};

    const std::pair<const char*, GLenum> shaders[] {
        { "shaders/lighting_vertex.glsl", GL_VERTEX_SHADER },
        { "shaders/lighting_fragment.glsl", GL_FRAGMENT_SHADER },
    };

    program.id = gl::load_shader_program(shaders);
    gl::link_shader_program(program.id);

    const auto transf_block_index = glGetUniformBlockIndex(program.id, "transformations");
    glUniformBlockBinding(program.id, transf_block_index, transf_binding_point);

    const auto lights_block_index = glGetUniformBlockIndex(program.id, "light_params");
    glUniformBlockBinding(program.id, lights_block_index, lights_binding_point);

    const auto mtl_block_index = glGetUniformBlockIndex(program.id, "material");
    glUniformBlockBinding(program.id, mtl_block_index, mtl_binding_point);

    glUseProgram(program.id);
    scope_exit({ glUseProgram(0); });

    for (auto i = 0; i < MAX_LIGHTS; ++i) {
        const auto uname = "u_shadow_maps[" + std::to_string(i) + ']';
        const auto uloc = glGetUniformLocation(program.id, uname.data());

        glUniform1i(uloc, FIRST_SM_TIU - GL_TEXTURE0 + i);
    }

    for (auto& uniform : lighting_program_uniforms) {
        program.*uniform.ptr = glGetUniformLocation(program.id, uniform.name);
    }

    return program;
}

#endif /* lighting_program_h */
