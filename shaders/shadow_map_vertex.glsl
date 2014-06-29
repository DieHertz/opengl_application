#version 330 core

layout(location = 0) in vec3 position_objectspace;

uniform mat4 depth_mvp_matrix;

void main() {
    gl_Position = depth_mvp_matrix * vec4(position_objectspace, 1);
}
