#version 330 core

layout(location = 0) in vec2 position_windowspace;

uniform mat4 window_to_clip_matrix;

void main() {
    gl_Position = window_to_clip_matrix * vec4(position_windowspace, 0, 1);
}
