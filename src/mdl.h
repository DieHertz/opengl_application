#ifndef mdl_h
#define mdl_h

#include <cstdint>

namespace mdl {

struct vec3 {
    float x, y, z;
};

static_assert(sizeof(vec3) == 3 * sizeof(float), "vec3 seems improperly aligned");

enum vertex_attrib_type {
    POSITION = 1,
    NORMAL = 2,
    TANGENT = 4,
    BITANGENT = 8,
    TEXTURE_COORDINATE = 16
};

struct header {
    uint64_t num_vertices;
    uint64_t num_indices;
    uint64_t type;
};

}

#endif /* mdl_h */
