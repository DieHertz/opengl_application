#version 330 core

precision mediump float;

in vec2 v_tex_coords;

layout(location = 0) out vec4 frag_color;

uniform vec4 color;
uniform bool is_font = false;
uniform sampler2D font;

void main() {
    if (is_font) {
        vec4 sample = texture(font, v_tex_coords);
        frag_color = vec4(color.rgb, sample.a);
    } else {
        frag_color = color;
    }
}
