#version 330 core

precision mediump float;

in vec2 v_tex_coord;

layout(location = 0) out float result;

uniform sampler2D u_sampler;
uniform float u_size;

void main() {
    float sum = 0.0;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y - 4 * u_size)).r * 0.05;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y - 3 * u_size)).r * 0.09;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y - 2 * u_size)).r * 0.12;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y - 1 * u_size)).r * 0.15;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y - 0 * u_size)).r * 0.16;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y + 1 * u_size)).r * 0.15;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y + 2 * u_size)).r * 0.12;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y + 3 * u_size)).r * 0.09;
    sum += texture(u_sampler, vec2(v_tex_coord.x, v_tex_coord.y + 4 * u_size)).r * 0.05;

    result = sum;
}
