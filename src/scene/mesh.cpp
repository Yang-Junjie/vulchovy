#include "scene/mesh.h"

#include <cmath>

namespace vulchovy {

void Aabb::extend(const vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

void Aabb::extend(const Aabb& other) {
    if (!other.valid())
        return;
    extend(other.min);
    extend(other.max);
}

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
            vertex.uv = vec2{u, v};
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
    const vec2 uvs[4] = {
        vec2{0.0f, 0.0f},
        vec2{1.0f, 0.0f},
        vec2{1.0f, 1.0f},
        vec2{0.0f, 1.0f},
    };
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = corners[i];
        vertex.normal = normal;
        vertex.uv = uvs[i];
        mesh.vertices.push_back(vertex);
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

void compute_smooth_normals(Mesh& mesh) {
    for (Vertex& vertex : mesh.vertices)
        vertex.normal = vec3{0.0f};

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t a = mesh.indices[i + 0];
        const uint32_t b = mesh.indices[i + 1];
        const uint32_t c = mesh.indices[i + 2];
        // The face normal cross product is not normalized, so its length scales
        // with the triangle area: the accumulation is area weighted.
        const vec3 face =
            cross(mesh.vertices[b].position - mesh.vertices[a].position,
                  mesh.vertices[c].position - mesh.vertices[a].position);
        mesh.vertices[a].normal += face;
        mesh.vertices[b].normal += face;
        mesh.vertices[c].normal += face;
    }

    for (Vertex& vertex : mesh.vertices) {
        const float len = length(vertex.normal);
        vertex.normal = len > 1e-16f ? vertex.normal / len : vec3{0.0f, 1.0f, 0.0f};
    }
}

Aabb mesh_bounds(const Mesh& mesh) {
    Aabb bounds;
    for (const Vertex& vertex : mesh.vertices)
        bounds.extend(vertex.position);
    return bounds;
}

} // namespace vulchovy
