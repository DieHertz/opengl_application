#version 410

in vec3 position;
out vec3 v_position;

uniform mat4 u_mv;
uniform mat4 u_mvp;

void main() {
    gl_Position = u_mvp * vec4(position, 1);
    v_position = position;
}
