#ifndef mesh_mesh_h
#define mesh_mesh_h

#include "gl/gl_include.h"
#include <glm/vec3.hpp>

namespace mesh {

struct mesh_data {
    GLenum primitive_mode;
    size_t num_indices;
    GLenum index_type;
    GLuint vao_id;
    GLuint vbo_ids[6];
};

mesh_data gen_sphere(float radius, int rings, int sectors);
mesh_data gen_quad(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 v4);
mesh_data load_mdl(const char* name);

} /* namespace mesh */

#endif /* mesh_mesh_h */
