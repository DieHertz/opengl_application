#version 410

out vec4 f_frag_data;
in vec3 v_position;

uniform sampler2D u_tex;

void main() {
    vec2 tex_coord = vec2(v_position.x, 0.5 - v_position.y);
    f_frag_data = texture(u_tex, tex_coord);
}
