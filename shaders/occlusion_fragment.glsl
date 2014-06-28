#version 330 core

precision mediump float;

in vec2 v_tex_coord;

layout(location = 0) out float occlusion;

uniform sampler2D u_noise_map;
uniform sampler2D u_normal_depth_map;

uniform int u_sample_count = 8;
uniform float tot_strength = 1.38;
uniform float strength = 0.07;
uniform float offset = 18.0;
uniform float falloff = 0.000002;
uniform float rad = 0.006;

const vec3 poisson_sphere[16] = vec3[](
    vec3(0.53812504, 0.18565957, -0.43192),
    vec3(0.13790712, 0.24864247, 0.44301823),
    vec3(0.33715037, 0.56794053, -0.005789503),
    vec3(-0.6999805, -0.04511441, -0.0019965635),
    vec3(0.06896307, -0.15983082, -0.85477847),
    vec3(0.056099437, 0.006954967, -0.1843352),
    vec3(-0.014653638, 0.14027752, 0.0762037),
    vec3(0.010019933, -0.1924225, -0.034443386),
    vec3(-0.35775623, -0.5301969, -0.43581226),
    vec3(-0.3169221, 0.106360726, 0.015860917),
    vec3(0.010350345, -0.58698344, 0.0046293875),
    vec3(-0.08972908, -0.49408212, 0.3287904),
    vec3(0.7119986, -0.0154690035, -0.09183723),
    vec3(-0.053382345, 0.059675813, -0.5411899),
    vec3(0.035267662, -0.063188605, 0.54602677),
    vec3(-0.47761092, 0.2847911, -0.0271716)
);

void main() {
    vec3 random_normal = normalize(texture(u_noise_map, v_tex_coord * offset).rgb * 2.0 - 1.0);

    vec4 current_sample = texture(u_normal_depth_map, v_tex_coord);
    float current_depth = current_sample.a;

    vec3 ep = vec3(v_tex_coord, current_depth);
    vec3 current_normal = current_sample.rgb;

    float bl = 0.0;
    float rad_d = rad / current_depth;

    for (int i = 0; i < u_sample_count; ++i) {
        vec3 ray = rad_d * reflect(poisson_sphere[i], random_normal);

        vec3 se = ep + sign(dot(ray, current_normal)) * ray;

        vec4 occluder_sample = texture(u_normal_depth_map, se.xy);

        float depth_diff = current_depth - occluder_sample.a;

        float norm_diff = 1 - dot(occluder_sample.rgb, current_normal);

        bl += step(falloff, depth_diff) * norm_diff * (1.0 - smoothstep(falloff, strength, depth_diff));
    }

    float ao = 1.0 - tot_strength * bl / float(u_sample_count);
    occlusion = ao;
}
