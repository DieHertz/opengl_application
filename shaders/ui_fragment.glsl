#version 330 core
#extension GL_ARB_explicit_uniform_location : enable

precision mediump float;

layout(location = 0) out vec4 frag_color;

layout(location = 0) uniform vec4 color;

void main() {
    frag_color = color;
}
