#include "scene/mesh.h"

#include <cmath>

namespace vulchovy {

Mesh make_uv_sphere(float radius, uint32_t segments, uint32_t rings) {
    Mesh mesh;

    const uint32_t segment_count = segments < 3 ? 3 : segments;
    const uint32_t ring_count = rings < 2 ? 2 : rings;

    mesh.vertices.reserve(static_cast<size_t>(ring_count + 1) * (segment_count + 1));
    for (uint32_t ring = 0; ring <= ring_count; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(ring_count);
        const float phi = glm::pi<float>() * v;
        const float sin_phi = std::sin(phi);
        const float cos_phi = std::cos(phi);

        for (uint32_t segment = 0; segment <= segment_count; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segment_count);
            const float theta = glm::two_pi<float>() * u;

            const vec3 normal{sin_phi * std::cos(theta), cos_phi, sin_phi * std::sin(theta)};
            Vertex vertex;
            vertex.position = normal * radius;
            vertex.normal = normal;
            mesh.vertices.push_back(vertex);
        }
    }

    const uint32_t stride = segment_count + 1;
    mesh.indices.reserve(static_cast<size_t>(ring_count) * segment_count * 6);
    for (uint32_t ring = 0; ring < ring_count; ++ring) {
        for (uint32_t segment = 0; segment < segment_count; ++segment) {
            const uint32_t i0 = ring * stride + segment;
            const uint32_t i1 = i0 + stride;
            const uint32_t i2 = i0 + 1;
            const uint32_t i3 = i1 + 1;

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);

            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i3);
        }
    }

    return mesh;
}

Mesh make_plane(float size, float y) {
    Mesh mesh;
    const float half = size * 0.5f;
    const vec3 normal{0.0f, 1.0f, 0.0f};

    const vec3 corners[4] = {
        vec3{-half, y, -half},
        vec3{half, y, -half},
        vec3{half, y, half},
        vec3{-half, y, half},
    };
    for (const vec3& corner : corners) {
        Vertex vertex;
        vertex.position = corner;
        vertex.normal = normal;
        mesh.vertices.push_back(vertex);
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

} // namespace vulchovy
