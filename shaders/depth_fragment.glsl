#version 330 core

in vec3 v_normal_eyespace;
in float v_linear_depth;

layout(location = 0) out vec4 normal_and_depth;

void main() {
    normal_and_depth = vec4(normalize(v_normal_eyespace), v_linear_depth);
}
