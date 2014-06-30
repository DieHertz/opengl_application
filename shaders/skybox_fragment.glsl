#version 330 core

precision mediump float;

in vec3 v_tex_coord;

uniform samplerCube u_map;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture(u_map, normalize(v_tex_coord));
}
