#pragma once

#include <cstdint>
#include <string>
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
    vec3 attenuation{1.0f, 1.0f, 1.0f}; // volume attenuation colour (tints transmission)
    int32_t base_color_texture = -1;
    int32_t metallic_roughness_texture = -1;
    int32_t normal_texture = -1;
    int32_t emissive_texture = -1;
};

/// @brief An RGBA8 texture uploaded to the GPU (sRGB for colour maps).
struct TextureData {
    std::vector<uint8_t> pixels; // width * height * 4 bytes
    uint32_t width = 0;
    uint32_t height = 0;
    bool srgb = true;
    bool valid() const { return width > 0 && height > 0; }
};

/// @brief CPU-side light: a point (radius 0) or a sphere area light.
struct Light {
    vec3 position{4.0f, 5.0f, 2.0f};
    /// @brief Emitted radiance (for an area light) or intensity (for a point
    ///        light, already folded with color).
    vec3 emission{20.0f, 20.0f, 20.0f};
    float radius = 0.0f;
};

/// @brief One drawable piece of a node: a mesh shaded by a material.
struct Primitive {
    uint32_t mesh = 0;
    uint32_t material = 0;
};

/// @brief A node of the scene graph.
///
/// Nodes form a forest through @c parent (index into Scene::nodes, -1 for a
/// root) and @c children. A node may draw zero or more primitives; its world
/// transform is the product of its local transform and all of its ancestors'.
struct Node {
    std::string name;
    mat4 local_transform{1.0f};
    int32_t parent = -1;
    std::vector<uint32_t> children;
    std::vector<Primitive> primitives;
};

/// @brief CPU-side description of everything the renderer traces.
struct Scene {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<TextureData> textures;
    std::vector<Node> nodes; // scene graph; the GPU instances are its flattening
    std::vector<Light> lights;
    Environment environment;
};

/// @brief World transform of a node (walking up its ancestors).
mat4 node_world_transform(const Scene& scene, uint32_t node);

/// @brief Bounding box of every primitive in the scene, in world space.
Aabb scene_bounds(const Scene& scene);

/// @brief Scene with a handful of spheres, distinct materials and two lights.
Scene make_default_scene();

} // namespace vulchovy
