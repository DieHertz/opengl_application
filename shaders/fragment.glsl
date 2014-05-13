#version 410

out vec4 f_frag_data;
in vec3 v_position;

uniform sampler2D s_tex;

void main() {
    vec2 tex_coord = vec2(v_position.x, 1 - v_position.y);
    f_frag_data = texture(s_tex, tex_coord);
}
