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

    compute_tangents(mesh);
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
    compute_tangents(mesh);
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

void compute_tangents(Mesh& mesh) {
    std::vector<vec3> tangent_accum(mesh.vertices.size(), vec3{0.0f});
    std::vector<vec3> bitangent_accum(mesh.vertices.size(), vec3{0.0f});

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t a = mesh.indices[i + 0];
        const uint32_t b = mesh.indices[i + 1];
        const uint32_t c = mesh.indices[i + 2];
        const Vertex& va = mesh.vertices[a];
        const Vertex& vb = mesh.vertices[b];
        const Vertex& vc = mesh.vertices[c];

        const vec3 edge1 = vb.position - va.position;
        const vec3 edge2 = vc.position - va.position;
        const vec2 delta1 = vb.uv - va.uv;
        const vec2 delta2 = vc.uv - va.uv;

        const float denom = delta1.x * delta2.y - delta2.x * delta1.y;
        if (std::abs(denom) < 1e-12f)
            continue;
        const float r = 1.0f / denom;
        const vec3 tangent = (edge1 * delta2.y - edge2 * delta1.y) * r;
        const vec3 bitangent = (edge2 * delta1.x - edge1 * delta2.x) * r;

        tangent_accum[a] += tangent;
        tangent_accum[b] += tangent;
        tangent_accum[c] += tangent;
        bitangent_accum[a] += bitangent;
        bitangent_accum[b] += bitangent;
        bitangent_accum[c] += bitangent;
    }

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const vec3 n = mesh.vertices[i].normal;
        vec3 t = tangent_accum[i] - n * dot(n, tangent_accum[i]); // Gram-Schmidt
        const float len = length(t);
        if (len > 1e-8f) {
            mesh.vertices[i].tangent = t / len;
        } else if (std::abs(n.y) < 0.999f) {
            mesh.vertices[i].tangent = normalize(cross(vec3{0.0f, 1.0f, 0.0f}, n));
        } else {
            mesh.vertices[i].tangent = normalize(cross(vec3{1.0f, 0.0f, 0.0f}, n));
        }
    }
}

} // namespace vulchovy
