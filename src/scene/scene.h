#pragma once

#include <cstdint>
#include <vector>

#include "core/math.h"
#include "scene/mesh.h"

namespace vulchovy {

/// @brief CPU-side physically based surface description (metallic-roughness).
struct Material {
    vec3 base_color{0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    vec3 emission{0.0f, 0.0f, 0.0f};
};

/// @brief CPU-side light: a point (radius 0) or a sphere area light.
struct Light {
    vec3 position{4.0f, 5.0f, 2.0f};
    /// @brief Emitted radiance (for an area light) or intensity (for a point
    ///        light, already folded with color).
    vec3 emission{20.0f, 20.0f, 20.0f};
    float radius = 0.0f;
};

/// @brief One object in the scene: a mesh placed by a transform, shaded by a
///        material.
struct Instance {
    uint32_t mesh_index = 0;
    uint32_t material_index = 0;
    mat4 transform{1.0f};
};

/// @brief CPU-side description of everything the renderer traces.
struct Scene {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Light> lights;
    std::vector<Instance> instances;
};

/// @brief Scene with a handful of spheres, distinct materials and two lights.
Scene make_default_scene();

} // namespace vulchovy
