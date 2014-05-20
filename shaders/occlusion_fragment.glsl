#version 330 core

precision mediump float;

in vec4 v_position_clipspace;
in vec3 v_normal_worldspace;

layout(location = 0) out vec3 frag_color;

layout(std140) uniform transformations {
    mat4 mvp_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

const int kernel_size = 64;

uniform sampler2D u_depth_map;
uniform sampler2D u_normal_map;
uniform sampler2D u_noise_map;

uniform vec2 u_noise_scale = vec2(640.0 / 4, 480.0 / 4);

uniform vec3 u_sample_kernel[kernel_size];

uniform float u_radius = 3.0f;

float linearize_depth(in float non_linear_depth) {
    return non_linear_depth;
    const float n = 0.1;
    const float f = 100.0;
    return 2 * n / (f + n - non_linear_depth * (f - n));
}

void main() {
    const vec4 ambient = vec4(0.9, 0.9, 0.9, 1);

    vec2 tex_coord = v_position_clipspace.xy / v_position_clipspace.w;

    float thfov = tan(60 / 2.0);
    float aspect = 640.0 / 480.0;

    vec3 view_ray = vec3(
        (tex_coord.x * 2.0 - 1.0) * thfov * aspect,
        (tex_coord.y * 2.0 - 1.0) * thfov,
        1.0
    );
    vec3 origin = view_ray * linearize_depth(texture(u_depth_map, tex_coord).x);
    vec3 normal = normalize(texture(u_normal_map, tex_coord).xyz * 2.0 - 1.0);

    vec3 rvec = texture(u_noise_map, tex_coord * u_noise_scale).xyz * 2.0 - 1.0;
    vec3 tangent = normalize(rvec - normal * dot(rvec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < kernel_size; ++i) {
        vec3 sample = tbn * u_sample_kernel[i] * u_radius + origin;

        vec4 offset = projection_matrix * vec4(sample, 1);
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        float sample_depth = linearize_depth(texture(u_depth_map, offset.xy).r);

        float range_check = float(abs(origin.z - sample_depth) < u_radius);
        occlusion += float(sample_depth <= sample.z) * range_check;
    }

    frag_color = vec3(1 - occlusion / kernel_size);
}
