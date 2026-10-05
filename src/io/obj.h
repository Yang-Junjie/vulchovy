#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "scene/scene.h"

namespace vulchovy {

/// @brief An imported model: meshes with the index of the material each uses.
struct ObjModel {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<uint32_t> mesh_material; // parallel to meshes, indexes into materials
};

/// @brief Loads a Wavefront OBJ (and its MTL) through tinyobjloader.
///
/// Missing vertex normals are computed as area-weighted, smooth normals. Returns
/// an empty model (and logs to stderr) when the file cannot be read.
ObjModel load_obj(const std::filesystem::path& path);

/// @brief Appends a model's meshes/materials into the scene and adds a node that
///        draws them with @p transform. Returns the new node's index.
uint32_t add_model(Scene& scene, const ObjModel& model, std::string name, const mat4& transform);

} // namespace vulchovy
