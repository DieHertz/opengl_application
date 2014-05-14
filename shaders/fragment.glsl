#version 410

out vec4 f_frag_color;

in vec3 v_position;
in vec3 v_normal;
in vec2 v_tex_coord;

uniform sampler2D u_diffuse_map;
uniform mat4 u_mv;
uniform mat4 u_mvp;
uniform vec3 u_eye_position;

void main() {
    f_frag_color = texture(u_diffuse_map, vec2(1, 1) - v_tex_coord);
}
