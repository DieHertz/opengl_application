#include "mesh.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <vector>

namespace mesh {

mesh_data gen_sphere(const float radius, const int rings, const int sectors) {
    const auto R = 1.0f / (rings - 1);
    const auto S = 1.0f / (sectors - 1);

    auto vertices = std::vector<glm::vec3>{};
    vertices.reserve(rings * sectors);
    auto normals = std::vector<glm::vec3>{};
    normals.reserve(rings * sectors);
    auto tex_coords = std::vector<glm::vec2>{};
    tex_coords.reserve(rings * sectors);

    for (auto r = 0; r < rings; ++r) {
        for (auto s = 0; s < sectors; ++s) {
            const auto y = std::sin(-glm::half_pi<float>() + glm::pi<float>() * r * R);
            const auto x = std::cos(2 * glm::pi<float>() * s * S) * std::sin(glm::pi<float>() * r * R);
            const auto z = std::sin(2 * glm::pi<float>() * s * S) * std::sin(glm::pi<float>() * r * R);

            vertices.emplace_back(x * radius, y * radius, z * radius);
            normals.emplace_back(x, y, z);
            tex_coords.emplace_back(s * S, r * R);
        }
    }

    auto indices = std::vector<GLuint>{};
    indices.reserve(6 * rings * sectors);

    for (auto r = 0; r < rings; ++r) {
        for (auto s = 0; s < sectors; ++s) {
            indices.push_back(r * sectors + s);
            indices.push_back(r * sectors + (s + 1));
            indices.push_back((r + 1) * sectors + (s + 1));
            indices.push_back(r * sectors + s);
            indices.push_back((r + 1) * sectors + (s + 1));
            indices.push_back((r + 1) * sectors + s);
        }
    }

    auto mesh = mesh_data{GL_TRIANGLES, indices.size()};

    glGenVertexArrays(1, &mesh.vao_id);
    glBindVertexArray(mesh.vao_id);

    glGenBuffers(sizeof(mesh.vbo_ids) / sizeof(mesh.vbo_ids[0]), mesh.vbo_ids);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_ids[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices.front()), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertices.front()), 0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_ids[1]);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(normals.front()), normals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(normals.front()), 0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_ids[2]);
    glBufferData(GL_ARRAY_BUFFER, tex_coords.size() * sizeof(tex_coords.front()), tex_coords.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(tex_coords.front()), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.vbo_ids[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices.front()), indices.data(), GL_STATIC_DRAW);

    return mesh;
}

} /* namespace mesh */
