#version 330 core

precision mediump float;

layout(location = 0) out vec4 frag_color;

uniform vec4 color;

void main() {
    frag_color = color;
}
