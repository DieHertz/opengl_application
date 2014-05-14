#ifndef mesh_mesh_h
#define mesh_mesh_h

#include "gl/gl_include.h"
#include <cstddef>

namespace mesh {

struct mesh_data {
    GLuint mode;
    std::size_t num_faces;
    GLuint vao_id;
    GLuint vbo_ids[4];
};

mesh_data gen_sphere(float radius, int rings, int sectors);

} /* namespace mesh */

#endif /* mesh_mesh_h */
