#include "mesh.h"
#include "geometry.h"
#include <vector>

namespace mesh {

mesh_data gen_sphere(const float radius, const int subdivs) {
    const auto r = radius / std::sqrt(3.0f);
    auto triangles = std::vector<triangle>{
        { { -r, -r, -r }, { r, r, -r }, { r, -r, -r } },
        { { -r, -r, -r }, { r, r, -r }, { -r, r, -r } },
        { { -r, r, -r }, { r, r, r }, { r, r, -r } },
        { { -r, r, -r }, { r, r, r }, { -r, r, r } },
        { { r, -r, -r }, { r, r, r }, { r, -r, r } },
        { { r, -r, -r }, { r, r, r }, { r, r, -r } },
        { { -r, -r, -r }, { r, -r, r }, { -r, -r, r } },
        { { -r, -r, -r }, { r, -r, r }, { r, -r, -r } },
        { { -r, -r, -r }, { -r, r, r }, { -r, r, -r } },
        { { -r, -r, -r }, { -r, r, r }, { -r, -r, r } },
        { { -r, -r, r }, { r, r, r }, { -r, r, r } },
        { { -r, -r, r }, { r, r, r }, { r, -r, r } },  
    };

    for (auto i = 0; i < subdivs; ++i) {
        auto subdivided_triangles = std::vector<triangle>{};
        subdivided_triangles.reserve(2 * triangles.size());

        for (auto& triangle : triangles) {
            auto mid = (triangle.a + triangle.b) / 2;
            mid = mid * length(triangle.a) / length(mid);
            subdivided_triangles.push_back({ triangle.a, triangle.c, mid });
            subdivided_triangles.push_back({ triangle.b, triangle.c, mid });
        }

        std::swap(triangles, subdivided_triangles);
    }

    GLuint vao_id;
    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    GLuint pos_id;
    glGenBuffers(1, &pos_id);
    glBindBuffer(GL_ARRAY_BUFFER, pos_id);
    glBufferData(GL_ARRAY_BUFFER, triangles.size() * sizeof(triangle), triangles.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);

    return { vao_id, pos_id, triangles.size() * sizeof(triangle) / sizeof(vertex::x) };
}

} /* namespace mesh */
