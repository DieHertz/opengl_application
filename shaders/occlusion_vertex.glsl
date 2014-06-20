#version 330 core

layout(location = 0) in vec2 pos_clipspace;

out vec2 v_tex_coord;

void main() {
    vec4 pos = vec4(pos_clipspace, 0, 1);
    gl_Position = pos;
    v_tex_coord = pos.xy * 0.5 + 0.5;
}
