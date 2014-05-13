#ifndef mesh_geometry_h
#define mesh_geometry_h

#include <cmath>

namespace mesh {

struct vertex { float x, y, z; };
struct triangle { vertex a, b, c; };

inline vertex operator+(const vertex& lhs, const vertex& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

inline vertex operator*(const vertex& v, const float f) {
    return { v.x * f, v.y * f, v.z * f };
}

inline vertex operator/(const vertex& v, const float f) {
    return { v.x / f, v.y / f, v.z / f };
}

inline float length(const vertex& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

} /* namespace mesh */

#endif /* mesh_geometry_h */
