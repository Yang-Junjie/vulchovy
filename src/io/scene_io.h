#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/math.h"
#include "scene/scene.h"

namespace vulchovy {

/// @brief A model reference placed in the scene (a scene file stores paths, not
///        geometry, so the file stays small).
struct ModelPlacement {
    std::string path;
    std::string name;
    mat4 transform{1.0f};
};

struct CameraSettings {
    vec3 position{0.0f, 3.0f, 9.0f};
    vec3 target{0.0f, 1.0f, 0.0f};
    vec3 up{0.0f, 1.0f, 0.0f};
    float vertical_fov_degrees = 40.0f;
};

struct EnvironmentSettings {
    std::string path;
    float intensity = 1.0f;
    float rotation = 0.0f;
};

/// @brief Serialisable description of a scene: external model references plus
///        lights, environment and camera. Meshes and materials are rebuilt by
///        loading the referenced models.
struct SceneDescription {
    EnvironmentSettings environment;
    CameraSettings camera;
    std::vector<ModelPlacement> models;
    std::vector<Light> lights;
};

/// @brief Loads a scene description from a JSON file (empty on failure).
SceneDescription load_scene_description(const std::filesystem::path& path);

/// @brief Writes a scene description to a JSON file.
bool save_scene_description(const SceneDescription& description,
                            const std::filesystem::path& path);

/// @brief Builds a runtime scene by importing every referenced model.
Scene build_scene(const SceneDescription& description);

/// @brief Description matching the built-in demo (environment + key light).
SceneDescription default_scene_description();

} // namespace vulchovy
