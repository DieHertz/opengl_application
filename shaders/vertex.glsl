#version 410

in vec3 position;
in vec3 normal;
in vec2 tex_coord;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_tex_coord;

uniform mat4 u_mv;
uniform mat4 u_mvp;

void main() {
    gl_Position = u_mvp * vec4(position, 1);

    v_position = position;
    v_normal = normal;
    v_tex_coord = tex_coord;
}
