#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_tex_coord;
out vec3 v_tangent;
out vec3 v_bitangent;

layout(std140) uniform transformations {
    mat4 depth_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

void main() {
    gl_Position = mvp_matrix * vec4(position, 1);

    v_position = position;
    v_normal = normal;
    v_tex_coord = tex_coord;
    v_tangent = tangent;
    v_bitangent = bitangent;
}
