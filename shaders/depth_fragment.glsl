#version 330 core

in vec3 v_normal;

layout(location = 0) out vec3 normal;

layout(std140) uniform transformations {
    mat4 depth_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

void main() {
    normal = normalize(/*normal_matrix * */v_normal);
}
