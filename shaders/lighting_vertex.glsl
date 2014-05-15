#version 410

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;

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
