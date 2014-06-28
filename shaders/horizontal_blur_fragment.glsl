#version 330 core

precision mediump float;

in vec2 v_tex_coord;

layout(location = 0) out float blurred;

uniform sampler2D u_sampler;
uniform float u_size;

void main() {
    float sum = 0.0;
    sum += texture(u_sampler, vec2(v_tex_coord.x - 4 * u_size, v_tex_coord.y)).r * 0.05;
    sum += texture(u_sampler, vec2(v_tex_coord.x - 3 * u_size, v_tex_coord.y)).r * 0.09;
    sum += texture(u_sampler, vec2(v_tex_coord.x - 2 * u_size, v_tex_coord.y)).r * 0.12;
    sum += texture(u_sampler, vec2(v_tex_coord.x - 1 * u_size, v_tex_coord.y)).r * 0.15;
    sum += texture(u_sampler, vec2(v_tex_coord.x - 0 * u_size, v_tex_coord.y)).r * 0.16;
    sum += texture(u_sampler, vec2(v_tex_coord.x + 1 * u_size, v_tex_coord.y)).r * 0.15;
    sum += texture(u_sampler, vec2(v_tex_coord.x + 2 * u_size, v_tex_coord.y)).r * 0.12;
    sum += texture(u_sampler, vec2(v_tex_coord.x + 3 * u_size, v_tex_coord.y)).r * 0.09;
    sum += texture(u_sampler, vec2(v_tex_coord.x + 4 * u_size, v_tex_coord.y)).r * 0.05;

    blurred = sum;
}
