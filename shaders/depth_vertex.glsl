#version 330 core

layout(location = 0) in vec3 position_objectspace;
layout(location = 1) in vec3 normal_objectspace;

out vec3 v_normal_eyespace;

layout(std140) uniform transformations {
    mat4 depth_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

uniform mat4 depth_mvp_matrix;

void main() {
    gl_Position = depth_mvp_matrix * vec4(position_objectspace, 1);
    v_normal_eyespace = normal_matrix * normal_objectspace;
}
