#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

out vec4 v_position_clipspace;

layout(std140) uniform transformations {
    mat4 mvp_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

void main() {
    v_position_clipspace = mvp_bias_matrix * vec4(position, 1);

    gl_Position = mvp_matrix * vec4(position, 1);
}
