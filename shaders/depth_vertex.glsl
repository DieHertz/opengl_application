#version 330 core

layout(location = 0) in vec3 position_objectspace;
layout(location = 1) in vec3 normal_objectspace;

out vec3 v_normal_eyespace;
out float v_linear_depth;

layout(std140) uniform transformations {
    mat4 depth_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

uniform mat4 depth_mvp_matrix;
uniform float u_near;
uniform float u_far;

float linearize_depth(in float non_linear_depth) {
    return 2 * u_near / (u_far + u_near - non_linear_depth * (u_far - u_near));
}

void main() {
    gl_Position = depth_mvp_matrix * vec4(position_objectspace, 1);
    v_normal_eyespace = normal_matrix * normal_objectspace;
    v_linear_depth = linearize_depth(gl_Position.z / gl_Position.w);
}
