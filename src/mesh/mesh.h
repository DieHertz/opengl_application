#ifndef mesh_mesh_h
#define mesh_mesh_h

#include "gl/gl.h"
#include <cstddef>

namespace mesh {

struct mesh_data {
    GLuint vao_id;
    GLuint pos_id;
    std::size_t num_vertices;
};

mesh_data gen_sphere(float radius, int subdivs);

} /* namespace mesh */

#endif /* mesh_mesh_h */
