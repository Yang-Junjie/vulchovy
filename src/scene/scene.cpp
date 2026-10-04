#include "scene/scene.h"

#include <glm/gtc/matrix_transform.hpp>

namespace vulchovy {

Scene make_default_scene() {
    Scene scene;

    scene.meshes.push_back(make_uv_sphere(1.0f, 64, 32)); // mesh 0
    scene.meshes.push_back(make_plane(16.0f, 0.0f));     // mesh 1

    scene.materials.push_back(Material{.base_color = vec3{0.7f, 0.7f, 0.68f}}); // ground
    scene.materials.push_back(Material{
        .base_color = vec3{0.9f, 0.15f, 0.1f}, .metallic = 0.0f, .roughness = 0.3f});
    scene.materials.push_back(Material{
        .base_color = vec3{0.9f, 0.75f, 0.25f}, .metallic = 1.0f, .roughness = 0.0f});
    scene.materials.push_back(Material{
        .base_color = vec3{0.2f, 0.6f, 0.9f}, .metallic = 1.0f, .roughness = 0.45f});
    scene.materials.push_back(Material{
        .base_color = vec3{0.95f, 0.95f, 0.95f}, .metallic = 0.0f, .roughness = 0.7f});

    // A single sphere area light.
    scene.lights.push_back(Light{
        .position = vec3{0.0f, 3.6f, 0.0f},
        .emission = vec3{18.0f, 18.0f, 18.0f},
        .radius = 0.7f,
    });

    scene.instances.push_back(Instance{.mesh_index = 1, .material_index = 0});
    scene.instances.push_back(Instance{
        .mesh_index = 0,
        .material_index = 1,
        .transform = glm::translate(mat4{1.0f}, vec3{-2.55f, 1.0f, 0.0f}),
    });
    scene.instances.push_back(Instance{
        .mesh_index = 0,
        .material_index = 2,
        .transform = glm::translate(mat4{1.0f}, vec3{-0.85f, 1.0f, 0.0f}),
    });
    scene.instances.push_back(Instance{
        .mesh_index = 0,
        .material_index = 3,
        .transform = glm::translate(mat4{1.0f}, vec3{0.85f, 1.0f, 0.0f}),
    });
    scene.instances.push_back(Instance{
        .mesh_index = 0,
        .material_index = 4,
        .transform = glm::translate(mat4{1.0f}, vec3{2.55f, 1.0f, 0.0f}),
    });

    return scene;
}

} // namespace vulchovy
