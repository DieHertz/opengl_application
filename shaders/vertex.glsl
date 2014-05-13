#version 410

in vec3 position;
out vec3 v_position;

void main() {
    gl_Position.xyz = position;
    v_position = position;
}
