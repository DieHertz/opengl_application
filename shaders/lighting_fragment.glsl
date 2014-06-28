#version 330 core

precision mediump float;

in vec3 v_position;
in vec3 v_normal;
in vec2 v_tex_coord;

layout(location = 0) out vec4 frag_color;

layout(std140) uniform transformations {
    mat4 depth_bias_matrix;
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat4 projection_matrix;
    mat3 normal_matrix;
};

struct light {
    vec4 pos;
    vec4 color;
};

const int MAX_LIGHTS = 8;
layout(std140) uniform light_params {
    light lights[MAX_LIGHTS];
    int num_lights;
};

layout(std140) uniform material {
    vec4 diffuse;
    vec4 specular;
    float shininess;
    float reflectance;
} mtl;

uniform sampler2D u_diffuse_map;
uniform sampler2D u_normal_map;
uniform samplerCube u_reflection_map;
uniform bool u_textured;

uniform bool u_occlusion;
uniform sampler2D u_occlusion_map;

uniform sampler2DShadow u_shadow_maps[MAX_LIGHTS];
uniform mat4 u_shadow_bias_matrices[MAX_LIGHTS];

const vec2 poisson_disk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

uniform int u_shadow_samples = 8;
uniform float u_shadow_distance = 600.0;
const float bias = 0;

vec3 transform_and_dehomogenize(in vec3 original) {
    vec4 transformed = mv_matrix * vec4(original, 1);
    return transformed.xyz / transformed.w;
}

float shadow_pcf(in int i, in vec3 uvd) {
    if (i == 0) return texture(u_shadow_maps[0], uvd);
    else if (i == 1) return texture(u_shadow_maps[1], uvd);
    else if (i == 2) return texture(u_shadow_maps[2], uvd);
    else if (i == 3) return texture(u_shadow_maps[3], uvd);
    else if (i == 4) return texture(u_shadow_maps[4], uvd);
    else if (i == 5) return texture(u_shadow_maps[5], uvd);
    else if (i == 6) return texture(u_shadow_maps[6], uvd);
    else if (i == 7) return texture(u_shadow_maps[7], uvd);
}

void main() {
    const vec3 eye_position = vec3(0, 0, 0);

    vec4 diffuse = mtl.diffuse + (u_textured ? texture(u_diffuse_map, vec2(1.0, 1.0) - v_tex_coord) : vec4(0));
    if (mtl.reflectance > 0) {
        diffuse = (1 - mtl.reflectance) * diffuse + mtl.reflectance * texture(u_reflection_map, v_normal);
    }
    vec4 ambient = vec4(0.1, 0.1, 0.1, 1.0);
    vec3 pos_dehomognized = transform_and_dehomogenize(v_position);
    vec3 eye_dir = normalize(eye_position - pos_dehomognized);
    vec3 normal = normalize(normal_matrix * v_normal);

    vec4 color = diffuse * 0.3;

    float occlusion = 1.0;
    if (u_occlusion) {
        vec4 tmp = mvp_matrix * vec4(v_position, 1);
        vec2 tex_coord = tmp.xy / tmp.w * 0.5 + 0.5;
        occlusion = texture(u_occlusion_map, tex_coord).r;
    }

    float visibility_per_sample = 0.8 / u_shadow_samples;

    for (int i = 0; i < num_lights; ++i) {
        vec4 shadow_coord = u_shadow_bias_matrices[i] * vec4(v_position, 1.0);
        vec3 shadow_coord_clipspace = shadow_coord.xyz / shadow_coord.w;
        float visibility = 1.0;

        for (int sample_idx = 0; sample_idx < u_shadow_samples; ++sample_idx) {
            int index = sample_idx;

            float sampled_shadow = float(shadow_pcf(i,
                vec3(shadow_coord_clipspace.xy + poisson_disk[index] / u_shadow_distance,
                (shadow_coord.z - bias) / shadow_coord.w)));

            visibility -= visibility_per_sample * (1.0 - sampled_shadow);
        }

        vec3 light_dir = normalize(transform_and_dehomogenize(lights[i].pos.xyz) - pos_dehomognized);
        vec3 half_vec = normalize(light_dir + eye_dir);

        color += visibility * diffuse * lights[i].color * max(dot(normal, light_dir), 0);
        if (mtl.shininess > 0) {
            color += visibility * mtl.specular * lights[i].color * pow(max(dot(normal, half_vec), 0), mtl.shininess);
        }
    }

    frag_color = color * occlusion;
}
