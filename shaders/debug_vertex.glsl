#version 330 core

layout(location = 0) in vec2 position_clipspace;

out vec2 v_tex_coord;

void main() {
    gl_Position = vec4(position_clipspace, -1, 1);
    v_tex_coord = position_clipspace * 0.5 + 0.5;
}
