#pragma once

#include <filesystem>

#include "core/math.h"
#include "scene/scene.h"

namespace vulchovy {

/// @brief Imports a glTF or GLB file (nodes, meshes, PBR materials and textures)
///        and appends it to the scene.
///
/// Root nodes are pre-multiplied by @p root_transform. Returns false when the
/// file cannot be parsed or contains no drawable geometry.
bool add_gltf(Scene& scene, const std::filesystem::path& path,
              const mat4& root_transform = mat4{1.0f});

} // namespace vulchovy
