#pragma once

#include <cstdint>
#include <vector>

#include "core/math.h"
#include "scene/environment.h"
#include "scene/mesh.h"

namespace vulchovy {

/// @brief CPU-side physically based surface description (metallic-roughness with
///        optional dielectric transmission).
struct Material {
    vec3 base_color{0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float anisotropy = 0.0f;   // 0 = isotropic, ->1 stretches the highlight along the tangent
    float transmission = 0.0f; // 0 = opaque, 1 = fully transmissive (glass)
    float ior = 1.5f;          // index of refraction for the dielectric
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
    Environment environment;
};

/// @brief Scene with a handful of spheres, distinct materials and two lights.
Scene make_default_scene();

} // namespace vulchovy
