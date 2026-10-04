#pragma once

#include <cstdint>
#include <vector>

#include "core/math.h"

namespace vulchovy {

/// @brief Interleaved vertex uploaded both to the ray tracing build input and
///        to a structured buffer read by the shading kernel.
///
/// The explicit padding makes the layout match std430 (a float3 is 16-byte
/// aligned there), so the CPU struct and the shader struct agree byte for byte.
struct Vertex {
    vec3 position;
    float padding0 = 0.0f;
    vec3 normal;
    float padding1 = 0.0f;
};

static_assert(sizeof(Vertex) == 32, "Vertex must match the std430 layout");

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

/// @brief Builds a UV sphere centred at the origin with smooth normals.
/// @param radius Sphere radius.
/// @param segments Longitude subdivisions.
/// @param rings Latitude subdivisions.
Mesh make_uv_sphere(float radius, uint32_t segments = 64, uint32_t rings = 32);

/// @brief Builds an axis-aligned plane on the XZ plane facing +Y.
/// @param size Side length of the square.
/// @param y Height of the plane.
Mesh make_plane(float size, float y = 0.0f);

} // namespace vulchovy
