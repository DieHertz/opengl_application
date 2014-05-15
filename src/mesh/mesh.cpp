#include "mesh.h"
#include "ext.h"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/detail/func_geometric.hpp>
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

    auto indices = std::vector<GLushort>{};
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

    auto mesh = mesh_data{GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT};

    glGenVertexArrays(1, &mesh.vao_id);
    glBindVertexArray(mesh.vao_id);

    glGenBuffers(array_length(mesh.vbo_ids), mesh.vbo_ids);

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

mesh_data gen_quad(const glm::vec3 v1, const glm::vec3 v2, const glm::vec3 v3, const glm::vec3 v4) {
    const glm::vec3 vertices[] { v1, v2, v3, v4 };
    const glm::vec3 normals[] {
        glm::normalize(glm::cross(v1 - v2, v1 - v4)),
        glm::normalize(glm::cross(v2 - v3, v2 - v1)),
        glm::normalize(glm::cross(v3 - v4, v3 - v2)),
        glm::normalize(glm::cross(v4 - v1, v4 - v3))
    };
    const glm::vec2 tex_coords[] { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    const GLushort indices[] { 0, 1, 2, 0, 2, 3 };

    auto mesh = mesh_data{GL_TRIANGLES, array_length(indices), GL_UNSIGNED_SHORT};

    glGenVertexArrays(1, &mesh.vao_id);
    glBindVertexArray(mesh.vao_id);

    glGenBuffers(array_length(mesh.vbo_ids), mesh.vbo_ids);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_ids[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertices[0]), 0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_ids[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(normals[0]), 0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_ids[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(tex_coords[0]), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.vbo_ids[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    return mesh;
}

} /* namespace mesh */
