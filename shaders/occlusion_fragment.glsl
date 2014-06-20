#version 330 core

precision mediump float;

in vec2 v_tex_coord;

layout(location = 0) out vec3 frag_color;

uniform sampler2D u_depth_map;
uniform sampler2D u_normal_map;

uniform float u_distance_treshold = 5.0f;
uniform float u_width = 640.0;
uniform float u_height = 480.0;
uniform float u_near = 1.0;
uniform float u_far = 30.0;
uniform float u_fov = 45.0;
uniform vec2 u_radius = vec2(5.0 / 640.0, 5.0 / 480.0);

const int sample_count = 16;
const vec2 poisson_disk[16] = vec2[](
    vec2( -0.94201624, -0.39906216 ),
    vec2( 0.94558609, -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2( 0.34495938, 0.29387760 ),
    vec2( -0.91588581, 0.45771432 ),
    vec2( -0.81544232, -0.87912464 ),
    vec2( -0.38277543, 0.27676845 ),
    vec2( 0.97484398, 0.75648379 ),
    vec2( 0.44323325, -0.97511554 ),
    vec2( 0.53742981, -0.47373420 ),
    vec2( -0.26496911, -0.41893023 ),
    vec2( 0.79197514, 0.19090188 ),
    vec2( -0.24188840, 0.99706507 ),
    vec2( -0.81409955, 0.91437590 ),
    vec2( 0.19984126, 0.78641367 ),
    vec2( 0.14383161, -0.14100790 )
);

float linearize_depth(in float non_linear_depth) {
    return 2 * u_near / (u_far + u_near - non_linear_depth * (u_far - u_near));
}

vec3 reconstruct_eyespace_position(in vec2 position_clipspace, in float linear_depth) {
    float thfov = tan(u_fov / 2.0);
    float aspect = u_width / u_height;

    vec3 view_ray = vec3(
        (position_clipspace.x * 2.0 - 1.0) * thfov * aspect,
        (position_clipspace.y * 2.0 - 1.0) * thfov,
        1.0);

    return view_ray * linear_depth;
}

void main() {
    float depth = texture(u_depth_map, v_tex_coord).r;
    vec3 view_pos = reconstruct_eyespace_position(v_tex_coord, linearize_depth(depth));

    vec3 view_normal = normalize(texture(u_normal_map, v_tex_coord).rgb * 2.0 - 1.0);

    float occlusion = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        vec2 sample_tex_coord = v_tex_coord + poisson_disk[i] * u_radius;
        float sample_depth = texture(u_depth_map, sample_tex_coord).r;

        vec3 sample_pos = reconstruct_eyespace_position(
            sample_tex_coord, linearize_depth(sample_depth));
        vec3 sample_dir = normalize(sample_pos - view_pos);

        float n_dot_s = max(dot(view_normal, sample_dir), 0);
        float vp_dist_sp = distance(view_pos, sample_pos);

        float a = 1.0 - smoothstep(u_distance_treshold, u_distance_treshold * 2, vp_dist_sp);

        occlusion += a * n_dot_s;
    }

    frag_color = vec3(1 - occlusion / sample_count);
}
