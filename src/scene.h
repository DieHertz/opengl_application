#ifndef scene_h
#define scene_h

#include "material.h"
#include "mesh/mesh.h"
#include "lighting_program.h"
#include "gl/gl_include.h"
#include <glm/vec4.hpp>
#include <vector>

struct scene_object {
    mesh::mesh_data mesh;
    material mtl;
    GLuint mtl_buffer_id;
    GLuint diffuse_tex_id;
    GLuint normal_tex_id;
    GLuint height_tex_id;
};

struct light {
    glm::vec4 pos;
    glm::vec4 color;
};

struct scene {
    std::vector<scene_object> objs;
    std::vector<light> lights;

    struct {
        mesh::mesh_data mesh;
        GLuint tex_id;
        GLuint program_id;
        GLuint map_loc;
    } skybox;

    lighting_program program;
};


#endif /* scene_h */
