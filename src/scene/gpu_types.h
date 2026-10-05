#pragma once

#include "core/math.h"

namespace vulchovy {

// GPU-side scene structs. They are mirrored by the declarations in
// shaders/common/scene.slang and must stay byte-for-byte compatible; every
// member is a vec4/ivec4 so the stride matches std430 without extra padding.

/// @brief Upper bound on the number of sampled textures the scene can bind. The
///        shader declares a matching fixed-size texture array.
inline constexpr uint32_t kMaxSceneTextures = 64;

/// @brief One entry of the material buffer (metallic-roughness / dielectric).
struct GpuMaterial {
    vec4 base_color; // rgb
    vec4 emission;   // rgb
    vec4 params;     // x = metallic, y = roughness, z = transmission, w = ior
    vec4 params2;    // x = anisotropy
    glm::ivec4 textures; // texture indices: x = base color, y = metal-rough, z = normal, w = emissive
};

/// @brief One entry of the light buffer.
struct GpuLight {
    vec4 position; // xyz = world position
    vec4 emission; // rgb = emitted radiance / intensity
    vec4 params;   // x = radius (0 = point light)
};

/// @brief One emissive triangle used as an area light for next-event estimation.
///
/// The emitted radiance is the material's emission (averaged over the emissive
/// texture) so that light sampling and BSDF hits stay consistent.
struct GpuMeshLight {
    vec4 p0;       // xyz = world position
    vec4 p1;       // xyz = world position
    vec4 p2;       // xyz = world position
    vec4 normal;   // xyz = geometric normal, w = area
    vec4 emission; // rgb = emitted radiance (linear)
};
static_assert(sizeof(GpuMeshLight) == 80, "GpuMeshLight must match std430");

/// @brief One entry of the instance buffer.
///
/// Stores the object-to-world and world-to-object transforms as explicit basis
/// vectors instead of a matrix, so the shader does not depend on matrix memory
/// layout conventions.
struct GpuInstance {
    vec4 to_world_x; // object-to-world basis for the object x axis
    vec4 to_world_y;
    vec4 to_world_z;
    vec4 to_world_t; // object-to-world translation
    vec4 to_object_x; // world-to-object basis for the world x axis
    vec4 to_object_y;
    vec4 to_object_z;
    vec4 to_object_t; // world-to-object translation
    glm::uvec4 geometry; // x = vertex offset, y = index offset, z = index count, w = material index
};

static_assert(sizeof(GpuMaterial) == 80, "GpuMaterial must match std430");
static_assert(sizeof(GpuLight) == 48, "GpuLight must match std430");
static_assert(sizeof(GpuInstance) == 144, "GpuInstance must match std430");

} // namespace vulchovy
