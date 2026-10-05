#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "core/math.h"

namespace vulchovy {

/// @brief Interleaved vertex uploaded both to the ray tracing build input and
///        to a structured buffer read by the shading kernel.
///
/// The explicit padding makes the layout match std430 (a float3/float2 is
/// 16/8-byte aligned there), so the CPU struct and the shader struct agree byte
/// for byte.
struct Vertex {
    vec3 position;
    float padding0 = 0.0f;
    vec3 normal;
    float padding1 = 0.0f;
    vec2 uv{0.0f, 0.0f};
    vec2 padding2{0.0f, 0.0f};
};

static_assert(sizeof(Vertex) == 48, "Vertex must match the std430 layout");

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

/// @brief Axis-aligned bounding box.
struct Aabb {
    vec3 min{std::numeric_limits<float>::max()};
    vec3 max{std::numeric_limits<float>::lowest()};

    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
    void extend(const vec3& point);
    void extend(const Aabb& other);
    vec3 center() const { return valid() ? (min + max) * 0.5f : vec3{0.0f}; }
    float radius() const { return valid() ? length(max - min) * 0.5f : 1.0f; }
};

/// @brief Builds a UV sphere centred at the origin with smooth normals.
Mesh make_uv_sphere(float radius, uint32_t segments = 64, uint32_t rings = 32);

/// @brief Builds an axis-aligned plane on the XZ plane facing +Y.
Mesh make_plane(float size, float y = 0.0f);

/// @brief Overwrites the normals with area-weighted, smooth vertex normals.
void compute_smooth_normals(Mesh& mesh);

/// @brief Bounding box of a mesh's positions.
Aabb mesh_bounds(const Mesh& mesh);

} // namespace vulchovy
