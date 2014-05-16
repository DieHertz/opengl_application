#version 410 core

layout(location = 0) out vec4 frag_color;

in vec3 v_position;
in vec3 v_normal;
in vec2 v_tex_coord;

layout(std140) uniform transformations {
    mat4 mv_matrix;
    mat4 mvp_matrix;
    mat3 normal_matrix;
};
uniform sampler2D u_diffuse_map;
uniform bool u_textured;
uniform vec4 u_diffuse;

struct light {
    vec4 pos;
    vec4 color;
};

vec3 dehomogenize(vec4 original) {
    return original.xyz / original.w;
}

vec3 transform_and_dehomogenize(vec3 original) {
    return dehomogenize(mv_matrix * vec4(original, 1));
}

void main() {
    vec4 diffuse = u_textured ? texture(u_diffuse_map, vec2(1, 1) - v_tex_coord) : u_diffuse;
    vec4 specular = vec4(1, 1, 1, 1);
    float shininess = 100;

    light light0 = light(vec4(1, 0, 1, 1), vec4(1, 1, 1, 1));
    light light1 = light(vec4(-5, 4, -3, 1), vec4(1, 0.5, 1, 1));
    light lights[2];
    lights[0] = light0;
    lights[1] = light1;
    int num_lights = 2;

    const vec3 eye_position = vec3(0, 0, 0 );

    vec3 pos_dehomognized = transform_and_dehomogenize(v_position);
    vec3 eye_dir = normalize(eye_position - pos_dehomognized);
    vec3 normal = normalize(normal_matrix * v_normal);

    vec4 color;

    for (int i = 0; i < num_lights; ++i) {
        vec3 light_dir = normalize(transform_and_dehomogenize(lights[i].pos.xyz) - pos_dehomognized);
        vec3 half_vec = normalize(light_dir + eye_dir);

        color += diffuse * lights[i].color * max(dot(normal, light_dir), 0);
        color += specular * lights[i].color * pow(max(dot(normal, half_vec), 0), shininess);
    }

    frag_color = color;
}
