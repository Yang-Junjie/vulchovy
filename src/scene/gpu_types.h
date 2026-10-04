#pragma once

#include "core/math.h"

namespace vulchovy {

// GPU-side scene structs. They are mirrored by the declarations in
// shaders/common/scene.slang and must stay byte-for-byte compatible; every
// member is a vec4/ivec4 so the stride matches std430 without extra padding.

/// @brief One entry of the material buffer (metallic-roughness).
struct GpuMaterial {
    vec4 base_color; // rgb
    vec4 emission;   // rgb
    vec4 params;     // x = metallic, y = roughness
};

/// @brief One entry of the light buffer.
struct GpuLight {
    vec4 position; // xyz = world position
    vec4 emission; // rgb = emitted radiance / intensity
    vec4 params;   // x = radius (0 = point light)
};

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

static_assert(sizeof(GpuMaterial) == 48, "GpuMaterial must match std430");
static_assert(sizeof(GpuLight) == 48, "GpuLight must match std430");
static_assert(sizeof(GpuInstance) == 144, "GpuInstance must match std430");

} // namespace vulchovy
