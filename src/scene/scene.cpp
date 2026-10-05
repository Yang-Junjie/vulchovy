#include "scene/scene.h"

#include <filesystem>

#include <glm/gtc/matrix_transform.hpp>

namespace vulchovy {

mat4 node_world_transform(const Scene& scene, uint32_t node) {
    mat4 world = scene.nodes[node].local_transform;
    int32_t parent = scene.nodes[node].parent;
    while (parent >= 0 && static_cast<size_t>(parent) < scene.nodes.size()) {
        world = scene.nodes[parent].local_transform * world;
        parent = scene.nodes[parent].parent;
    }
    return world;
}

Aabb scene_bounds(const Scene& scene) {
    Aabb bounds;
    for (uint32_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        const Node& node = scene.nodes[node_index];
        if (node.primitives.empty())
            continue;
        const mat4 world = node_world_transform(scene, node_index);
        for (const Primitive& primitive : node.primitives) {
            if (primitive.mesh >= scene.meshes.size())
                continue;
            const Aabb local = mesh_bounds(scene.meshes[primitive.mesh]);
            if (!local.valid())
                continue;
            for (int corner = 0; corner < 8; ++corner) {
                const vec3 point{(corner & 1) ? local.max.x : local.min.x,
                                 (corner & 2) ? local.max.y : local.min.y,
                                 (corner & 4) ? local.max.z : local.min.z};
                bounds.extend(vec3(world * vec4{point, 1.0f}));
            }
        }
    }
    return bounds;
}

Scene make_default_scene() {
    Scene scene;

    scene.meshes.push_back(make_uv_sphere(1.0f, 64, 32)); // mesh 0
    scene.meshes.push_back(make_plane(16.0f, 0.0f));     // mesh 1

    scene.materials.push_back(Material{
        .base_color = vec3{0.75f, 0.75f, 0.78f},
        .metallic = 1.0f,
        .roughness = 0.28f}); // frosted metal ground
    scene.materials.push_back(Material{
        .base_color = vec3{0.9f, 0.15f, 0.1f}, .metallic = 0.0f, .roughness = 0.3f});
    scene.materials.push_back(Material{
        .base_color = vec3{0.9f, 0.75f, 0.25f}, .metallic = 1.0f, .roughness = 0.0f});
    scene.materials.push_back(Material{
        .base_color = vec3{1.0f, 1.0f, 1.0f},
        .metallic = 0.0f,
        .roughness = 0.18f,
        .transmission = 1.0f,
        .ior = 1.5f}); // frosted glass
    scene.materials.push_back(Material{
        .base_color = vec3{0.2f, 0.6f, 0.9f},
        .metallic = 1.0f,
        .roughness = 0.35f,
        .anisotropy = 0.8f});
    scene.materials.push_back(Material{
        .base_color = vec3{0.95f, 0.95f, 0.95f}, .metallic = 0.0f, .roughness = 0.7f});

    // A single sphere area light (a key light on top of the environment).
    scene.lights.push_back(Light{
        .position = vec3{0.0f, 3.6f, 0.0f},
        .emission = vec3{6.0f, 6.0f, 6.0f},
        .radius = 0.7f,
    });

    auto add_node = [&scene](std::string name, uint32_t mesh, uint32_t material,
                             const mat4& transform) {
        Node node;
        node.name = std::move(name);
        node.local_transform = transform;
        node.primitives.push_back(Primitive{.mesh = mesh, .material = material});
        scene.nodes.push_back(std::move(node));
    };

    add_node("ground", 1, 0, mat4{1.0f});
    add_node("red", 0, 1, glm::translate(mat4{1.0f}, vec3{-4.4f, 1.0f, 0.0f}));
    add_node("gold", 0, 2, glm::translate(mat4{1.0f}, vec3{-2.2f, 1.0f, 0.0f}));
    add_node("glass", 0, 3, glm::translate(mat4{1.0f}, vec3{0.0f, 1.0f, 0.0f}));
    add_node("blue", 0, 4, glm::translate(mat4{1.0f}, vec3{2.2f, 1.0f, 0.0f}));
    add_node("white", 0, 5, glm::translate(mat4{1.0f}, vec3{4.4f, 1.0f, 0.0f}));

    // HDR environment (image-based lighting). Falls back to a procedural sky
    // when the asset is unavailable so the renderer still runs.
    scene.environment = load_environment(
        std::filesystem::path{VULCHOVY_ASSET_DIR} / "modern_buildings_2_2k.hdr");
    if (!scene.environment.valid())
        scene.environment = make_procedural_sky();
    scene.environment.intensity = 1.0f;
    scene.environment.rotation = 0.0f;

    return scene;
}

} // namespace vulchovy
