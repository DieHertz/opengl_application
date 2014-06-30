#version 330 core

layout(location = 0) in vec3 position;

out vec3 v_tex_coord;

layout(std140) uniform transformations {
    mat4 depth_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

void main() {
    gl_Position = mvp_matrix * vec4(50 * position, 1);
    v_tex_coord = position;
}
